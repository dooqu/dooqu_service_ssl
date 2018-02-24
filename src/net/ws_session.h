#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

#include <cstdarg>
#include <string>
#include <iostream>
#include <memory>
#include <mutex>
#include <functional>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "../basic/ws_client.h"
#include "service_error.h"
#include "ws_framedata_parser.h"
#include "ws_request_parser.h"
#include "buffer_stream.h"
#include "../util/ws_util.h"

namespace dooqu_service
{
namespace net
{

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace dooqu_service::basic;

typedef boost::asio::ip::tcp::socket tcp_stream;
typedef boost::asio::ssl::stream<tcp_stream> ssl_stream;

template<class SOCK_TYPE>
class ws_server;

template <class SOCK_TYPE>
class ws_session : public ws_client, public std::enable_shared_from_this<ws_session<SOCK_TYPE>>, boost::noncopyable
{
	typedef std::shared_ptr<ws_session<SOCK_TYPE>> ws_session_ptr;
	friend class ws_server<SOCK_TYPE>;
  public:
	enum
	{
		MAX_BUFFER_SIZE = 128,
		MAX_BUFFER_SEQUENCE_SIZE = 32,
		MAX_BUFFER_SIZE_DOUBLE_TIMES = 4
	};

  protected:
	//发送数据缓冲区
	std::vector<buffer_stream *> send_buffer_sequence_;
	//正在处理的位置，这个位置是发送函数正在读取的位置
	int read_pos_;
	//write_pos_永远指向该写入的位置
	int write_pos_;
	//数据锁
	std::recursive_mutex send_lock_;

	std::recursive_mutex recv_lock_;

	int error_code_;

	bool is_error_;

	unsigned short error_frame_sended_;
	unsigned short error_frame_recved_;
	//接收握手数据的缓冲区，指向ws_framedata内的buffer
	char *recv_buffer;
	//io_service对象
	io_service &ios;
	//包含的socket对象
	SOCK_TYPE socket_;

	ws_request request_;

	ws_request_parser req_parser_;

	ws_framedata frame_data_;

	ws_framedata_parser data_parser_;

	//用来标识socket对象是否可用
	bool available_;

	std::recursive_mutex &get_recv_mutex()
	{
		return this->recv_lock_;
	}

	std::recursive_mutex &get_send_mutex()
	{
		return this->send_lock_;
	}

	//客户端的启动函数
	void start()
	{
		socket_.async_read_some(boost::asio::buffer(this->recv_buffer, ws_framedata::BUFFER_SIZE),
								std::bind(&ws_session<SOCK_TYPE>::ws_handshake_read_handle, shared_from_this(),
										  std::placeholders::_1, std::placeholders::_2));
	}

	void ws_handshake_read_handle(const boost::system::error_code &error, size_t bytes_received)
	{
		ws_session_ptr self = shared_from_this();
		if (!error)
		{
			request_.content_size += bytes_received;
			ws_request_parser::parse_result ret = req_parser_.parse(request_, recv_buffer, recv_buffer + bytes_received);

			if (ret == ws_request_parser::parse_result_ok)
			{
				//读取在缓冲区未读的数据
				size_t bytes_readed = 0;
				size_t bytes_to_read = socket_.lowest_layer().available();
				if (bytes_to_read > 0)
				{
					while (bytes_to_read > 0)
					{
						bytes_to_read -= boost::asio::read(socket_, boost::asio::buffer(recv_buffer, ws_framedata::BUFFER_SIZE));
					}
				}
				//确认握手请求的信息是否正确，并响应
				if (confirm_ws_handshake_and_response(request_))
				{
					this->read_from_client();
					return;
				};
			}
			else if (ret == ws_request_parser::parse_result_indeterminate)
			{
				//buffer max length is 8kbyte.
				if (request_.content_size <= 8 * 1024)
				{
					socket_.async_read_some(boost::asio::buffer(recv_buffer, ws_framedata::BUFFER_SIZE),
											std::bind(&ws_session<SOCK_TYPE>::ws_handshake_read_handle, self,
													  std::placeholders::_1,
													  std::placeholders::_2));
					return;
				}
			}
		}

		const char *bad_request_400 = "HTTP/1.1 400 Bad Request\r\n\r\n";
		boost::asio::async_write(this->socket_, boost::asio::buffer(bad_request_400, strlen(bad_request_400)),
								 [this, self](const boost::system::error_code &error, size_t bytes_sended) {
									 //___lock___(this->recv_lock_, "handshake");
									 this->on_error(dooqu_service::net::service_error::WS_HANDSHAKE_ERROR);
								 });
	}
	//开始从客户端读取数据
	void read_from_client()
	{
		this->socket_.async_read_some(boost::asio::buffer(this->frame_data_.data, ws_framedata::BUFFER_SIZE),
								  std::bind(&ws_session<SOCK_TYPE>::on_data_received, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
	}

	//用来接收数据的回调方法
	virtual void on_data_received(const boost::system::error_code &error, size_t bytes_received)
	{
		if (!error)
		{
			do
			{
				___lock___(this->recv_lock_, "ws_client::on_data_received");
				framedata_parse_result result = data_parser_.parse(frame_data_, bytes_received);
				if (result == framedata_ok)
				{
					if (frame_data_.opcode_ == ws_framedata::opcode::CLOSE)
					{
						this->error_frame_recved_ = 1;
						if (this->error_frame_sended_ == 0)
						{
							//client first send the closure request.
							//then server send the confirm closure frame.
							//after write handle complete, server should close the socket.
							this->write_error(frame_data_.code, frame_data_.reason);
						}
						else if (this->error_frame_sended_ == 2)
						{
							//in this case, server first send the closure frame,and client confirmed.
							//client send this confirm closure frame.at this time,server should shutdown low_layer socket immediately.
							this->async_close();
						}
					}
					else if (this->error_code_ == service_error::NO_ERROR)
					{
						this->on_frame_data(&frame_data_);
					}
					
					//粘包，继续解析
					if (frame_data_.pos_ < frame_data_.length)
					{
						//std::cout << "数据粘包，继续解析" << std::endl;
						frame_data_.length = 0 ;
						frame_data_.state_ = ws_framedata::ready;						
						continue;
					}
					frame_data_.reset();
				}
				else if (result == framedata_indeterminate)
				{
					if (frame_data_.start_pos_ == 0)
					{
						//溢出，缓冲区装不下一个完整frame数据
						if (bytes_received == ws_framedata::BUFFER_SIZE)
						{
							this->on_error(dooqu_service::net::service_error::DATA_OUT_OF_BUFFER);
							return;
						}
					}
					else
					{
						//std::cout << "半包数据移动" << std::endl;
						//半包数据向前移动
						frame_data_.length = frame_data_.length - frame_data_.start_pos_;
						memcpy(frame_data_.data, &frame_data_.data[frame_data_.start_pos_], frame_data_.length);
						frame_data_.pos_ = frame_data_.pos_ - frame_data_.start_pos_;
						frame_data_.data_pos_ = frame_data_.data_pos_ - frame_data_.start_pos_;
						frame_data_.start_pos_ = 0;
					}
				}
				else if (result == framedata_error)
				{
					frame_data_.reset();
				}
				break;
			} while (true);

			socket_.async_read_some(boost::asio::buffer(this->frame_data_.data + frame_data_.length, ws_framedata::BUFFER_SIZE - frame_data_.length),
									std::bind(&ws_session<SOCK_TYPE>::on_data_received, shared_from_this(),
											  std::placeholders::_1, std::placeholders::_2));
		}
		else
		{
			this->set_available(false);
			this->on_error(dooqu_service::net::service_error::WS_ERROR_NORMAL_CLOSURE);
		}
	}

	bool confirm_ws_handshake_and_response(ws_request &req)
	{
		bool upgrade_header = false, connect_header = false;
		const char *key = NULL, key1 = NULL, key2 = NULL;

		for (int i = 0, j = req.headers.size(); i < j; i++)
		{
			if (strcasecmp(req.headers.at(i).name.c_str(), "Sec-WebSocket-Key") == 0)
			{
				key = req.headers.at(i).value.c_str();
			}
			if (upgrade_header == false && strcasecmp(req.headers.at(i).name.c_str(), "Upgrade") == 0 && strcasecmp(req.headers.at(i).value.c_str(), "websocket") == 0)
			{
				upgrade_header = true;
			}
			if (connect_header == false && strcasecmp(req.headers.at(i).name.c_str(), "Connection") == 0 && strcasecmp(req.headers.at(i).value.c_str(), "Upgrade") == 0)
			{
				connect_header = true;
			}
			if (connect_header && upgrade_header)
			{
				if (key != NULL)
				{
					goto _label_mode_1;
				}
				else if (key1 != NULL && key2 != NULL)
				{
					goto _label_mode_2;
				}
			}
		}

		_label_mode_1:
		{
			if (this->on_ws_handshake(&req) != 0)
			{
				return false;
			}
			char key_data[128] = {0};
			sprintf(key_data, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);

			char accept_key[29] = {0};
			dooqu_service::util::ws_util::generate(key, accept_key);

			this->write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", accept_key);

			return true;
		}

		_label_mode_2:
		{
			return false;
		}
		return false;
	}

	//当有数据到达时会被调用，该方法为虚方法，由子类进行具体逻辑的处理和运用
	//inline virtual void on_data(char* client_data) = 0;
	//当客户端出现规则错误时进行调用， 该方法为虚方法，由子类进行具体的逻辑的实现。
	unsigned short get_error_code()
	{
		return this->error_code_;
	}

	void set_error_code(unsigned short err_code)
	{
		this->error_code_ = err_code;
	}

	virtual void on_error(const int error) = 0;

	virtual void on_frame_data(ws_framedata *framedata) = 0;

	virtual int on_ws_handshake(ws_request *request)
	{
		return dooqu_service::net::service_error::NO_ERROR;
	}

	virtual bool alloc_available_buffer(buffer_stream **buffer_alloc)
	{
		//如果当前的写入位置的指针指向存在一个可用的send_buffer，那么直接取这个集合；
		if (write_pos_ < this->send_buffer_sequence_.size())
		{
			//std::cout <<this << "使用已有的buffer_stream at MAIN:" << this->send_buffer_sequence_.size() << std::endl;
			*buffer_alloc = this->send_buffer_sequence_.at(this->write_pos_);
			this->write_pos_++;
			return true;
		}
		else
		{
			//如果指向的位置不存在，需要新申请对象；
			//如果已经存在了超过16个对象，说明网络异常那么断开用户；不再新申请数据;
			if (this->send_buffer_sequence_.size() >= MAX_BUFFER_SEQUENCE_SIZE)
			{
				*buffer_alloc = NULL;
				return false;
			}

			buffer_stream *curr_buffer = buffer_stream::create(MAX_BUFFER_SIZE);
			assert(curr_buffer != NULL);
			if (curr_buffer == NULL)
				return NULL;

			this->send_buffer_sequence_.push_back(curr_buffer);
			std::cout << this << " 创建buffer_stream " << this->send_buffer_sequence_.size() << std::endl;
			//得到当前的send_buffer/
			//this->write_pos_ = this->send_buffer_sequence_.size() - 1;
			*buffer_alloc = this->send_buffer_sequence_.at(this->write_pos_);
			//将写入指针指向下一个预置位置
			this->write_pos_++;
			return true;
		}
	}

		//发送数据的回调方法
	void write_handle(const boost::system::error_code &error)
	{
		buffer_stream *curr_buffer = this->send_buffer_sequence_.at(this->read_pos_);
		if (curr_buffer->is_error_frame())
		{
			___lock___(this->recv_lock_, "ssl_connection::send_handle::send_buffer_lock_");
			this->error_frame_sended_ = 2;
			if (this->error_frame_recved_ == 1)
			{
				this->async_close();
			}
		}

		___lock___(this->send_lock_, "ssl_connection::send_handle::send_buffer_lock_");
		if (error || ++read_pos_ >= write_pos_ || error_frame_sended_ == 2)
		{
			//if an error occured or the messages in the sequencen is all sended, then reset and return.
			read_pos_ = -1;
			write_pos_ = 0;
		}
		else
		{			
			//std::cout << "WRITE:" << curr_buffer->read() + curr_buffer->pos_start + 2 << std::endl;
			//if not error and has remain buffers to send,then continue.
			buffer_stream *curr_buffer = this->send_buffer_sequence_.at(this->read_pos_);
			boost::asio::async_write(this->socket_, boost::asio::buffer(curr_buffer->read() + curr_buffer->pos_start, curr_buffer->size()),
									 std::bind(&ws_session<SOCK_TYPE>::write_handle, shared_from_this(), std::placeholders::_1));
		}
	}

  public:
	ws_session(io_service&);
	ws_session(io_service&, boost::asio::ssl::context&);
	virtual ~ws_session()
	{
		std::cout << "~ws_client" << std::endl;
		int size = this->send_buffer_sequence_.size();
		for (int i = 0; i < size; i++)
		{
			buffer_stream *curr_buffer = this->send_buffer_sequence_.at(i);
			buffer_stream::destroy(curr_buffer);
		}
		this->send_buffer_sequence_.clear();
	}

	SOCK_TYPE& socket()
	{
		return this->socket_;
	}

	void set_available(bool is_avaibled)
	{
		this->available_ = is_avaibled;
	}

	bool is_availabled()
	{
		return this->available_;
	}

	bool available()
	{
		return this->available_;
	}

	//向客户端输出数据字节流方法，此方法只提供向用户写入裸流，不报装websocket数据协议
	void write(const char *format, ...)
	{
		buffer_stream *curr_buffer = NULL;

		___lock___(this->send_lock_, "ws_client::write::send_lock_");
		if (this->alloc_available_buffer(&curr_buffer) == false)
		{
			return;
		}

		for (;;)
		{
			va_list arg_ptr;
			va_start(arg_ptr, format);
			int bytes_writed = curr_buffer->write(format, arg_ptr);
			va_end(arg_ptr);

			//-1是出错了， 有可能回一直返回-1,不是长度问题，造成死循环
			if (bytes_writed > 0)
			{
				break;
			}
			else if (bytes_writed == 0)
			{
				curr_buffer->double_size();
				continue;
			}
			else if (bytes_writed < 0)
			{
				//分配失败
				return;
			}
		}

		//std::cout << "缓冲区为" << curr_buffer->read() << std::endl;
		if (read_pos_ == -1)
		{
			//只要read_pos_ == -1，说明write没有在处理任何数据，说明没有处于发送状态
			++read_pos_;
			boost::asio::async_write(this->socket_, boost::asio::buffer(curr_buffer->read() + curr_buffer->pos_start, curr_buffer->size()),
									 std::bind(&ws_session<SOCK_TYPE>::write_handle, shared_from_this(), std::placeholders::_1));
		}
	}

	void write(char *data)
	{
		this->write("%s", data);
	}

	void write_frame(bool isFin, ws_framedata::opcode opcode, const char *format, ...)
	{
		buffer_stream *curr_buffer = NULL;

		___lock___(this->send_lock_, "ssl_connection::write::send_buffer_lock_");
		bool ret = this->alloc_available_buffer(&curr_buffer);
		assert(ret == true);
		if (ret == false)
		{
			return;
		}

		for (;;)
		{
			va_list arg_ptr;
			va_start(arg_ptr, format);
			int bytes_writed = curr_buffer->write_frame(format, arg_ptr);
			va_end(arg_ptr);

			//-1是出错了， 有可能回一直返回-1,不是长度问题，造成死循环
			assert(bytes_writed >= 0);
			if (bytes_writed > 0)
			{
				break;
			}
			else if (bytes_writed == 0)
			{
				curr_buffer->double_size();
				continue;
			}
			else if (bytes_writed < 0)
			{
				//分配失败
				return;
			}
		}

		unsigned finbyte = (isFin) ? 1 : 0;
		finbyte = (finbyte << 7) | (unsigned char)opcode;
		*curr_buffer->at(curr_buffer->pos_start) = finbyte;
		curr_buffer->set_error_frame(opcode == ws_framedata::opcode::CLOSE);

		//std::cout << "缓冲区为" << curr_buffer->read() << std::endl;
		if (read_pos_ == -1)
		{
			//只要read_pos_ == -1，说明write没有在处理任何数据，说明没有处于发送状态
			++read_pos_;
			//std::cout << "WRITE:" << (curr_buffer->read(curr_buffer->pos_start + 2)) << std::endl;
			boost::asio::async_write(this->socket_, boost::asio::buffer(curr_buffer->read() + curr_buffer->pos_start, curr_buffer->size()),
									 std::bind(&ws_session<SOCK_TYPE>::write_handle, shared_from_this(), std::placeholders::_1));
		}
	}

	void write_error(uint16_t error_code, char *error_reason = NULL)
	{
		if (this->error_frame_sended_ == 0)
		{
			this->error_frame_sended_ = 1;
			//reset client's error code.
			this->error_code_ = error_code;
			//fill the error code to buffer.
			char buff_err_code[3] = {0};
			ws_util::fill_net_buf_by_int16(buff_err_code, error_code);

			this->write_frame(true, ws_framedata::opcode::CLOSE, "%s%s", buff_err_code, error_reason);
		}
	}

	void async_close()
	{
		boost::system::error_code err_code;
		this->socket().shutdown(boost::asio::socket_base::shutdown_both);
		this->socket().close();
	}
};

template <>
inline ws_session<tcp_stream>::ws_session(io_service &ios) : ios(ios),
													 socket_(ios),
													 error_code_(dooqu_service::net::service_error::NO_ERROR),
													 recv_buffer(frame_data_.data),
													 read_pos_(-1),
													 write_pos_(0),
													 is_error_(false),
													 error_frame_recved_(0),
													 error_frame_sended_(0),
													 available_(false)
{
	for (int i = 0; i < 8; i++)
	{
		buffer_stream *bs = buffer_stream::create(ws_session<tcp_stream>::MAX_BUFFER_SIZE);
		if (bs != NULL)
		{
			this->send_buffer_sequence_.push_back(bs);
		}
	}
}

template <>
inline ws_session<ssl_stream>::ws_session(io_service &ios, boost::asio::ssl::context &ssl_context) : ios(ios),
																		  socket_(ios, ssl_context),
																		  error_code_(dooqu_service::net::service_error::NO_ERROR),
																		  recv_buffer(frame_data_.data),
																		  read_pos_(-1),
																		  write_pos_(0),
																		  is_error_(false),
																		  error_frame_recved_(0),
																		  error_frame_sended_(0),
																		  available_(false)
{
	for (int i = 0; i < 8; i++)
	{
		buffer_stream *bs = buffer_stream::create(ws_session<ssl_stream>::MAX_BUFFER_SIZE);
		if (bs != NULL)
		{
			this->send_buffer_sequence_.push_back(bs);
		}
	}
}

//start函数的ssl_stream的特化版本
template <>
inline void ws_session<ssl_stream>::start()
{
	ws_session_ptr self = shared_from_this();
	socket_.async_handshake(
		boost::asio::ssl::stream_base::server,
		[this, self](const boost::system::error_code &error) {
			if (!error)
			{
				socket_.async_read_some(boost::asio::buffer(this->recv_buffer, ws_framedata::BUFFER_SIZE),
										std::bind(&ws_session<ssl_stream>::ws_handshake_read_handle, shared_from_this(),
												  std::placeholders::_1, std::placeholders::_2));
			}
			else
			{
				//when error at ssl handshake, simply close the connection.
				this->on_error(dooqu_service::net::service_error::SSL_HANDSHAKE_ERROR);
			}
		});
}

//针对ssl_stream特化的async_close实现
template <>
inline void ws_session<ssl_stream>::async_close()
{
	//___lock___(this->recv_lock_, "game_client::disconnect.recv_lock_");
	boost::system::error_code err_code;
	ws_session_ptr self = shared_from_this();
	this->socket().async_shutdown([this, self](const boost::system::error_code &error) {
		this->socket().lowest_layer().close();
	});
}

}
}

#endif
