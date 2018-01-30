#include "ws_client.h"

namespace dooqu_service
{
	namespace net
	{
		ws_client::ws_client(io_service& ios, boost::asio::ssl::context& ssl_context) : ios(ios),
			socket_(ios, ssl_context),
			error_code_(dooqu_service::net::service_error::CLIENT_NET_ERROR),
			recv_buffer(frame_data_.data),
			read_pos_(-1),
			write_pos_(0),
			is_error_(false),
			available_(false)
		{
			for (int i = 0; i < 8; i++)
			{
				buffer_stream* bs = buffer_stream::create(MAX_BUFFER_SIZE);
				if (bs != NULL)
				{
					this->send_buffer_sequence_.push_back(bs);
				}
			}
		}

		void ws_client::start()
		{
			ws_client_ptr self = shared_from_this();
			socket_.async_handshake(boost::asio::ssl::stream_base::server,
				[this, self](const boost::system::error_code& error)
			{
				if (!error)
				{
					socket_.async_read_some(boost::asio::buffer(this->recv_buffer, ws_framedata::BUFFER_SIZE),
						std::bind(&ws_client::ws_handshake_read_handle, shared_from_this(),
							std::placeholders::_1, std::placeholders::_2));
				}
				else
				{
					___lock___(this->recv_lock_, "start");
					this->on_error(dooqu_service::net::service_error::SSL_HANDSHAKE_ERROR);
				}
			});
		}


		void ws_client::ws_handshake_read_handle(const boost::system::error_code& error, size_t bytes_received)
		{
			ws_client_ptr self = shared_from_this();
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
					//buffer max size is 8k.
					if (request_.content_size <= 8 * 1024)
					{
						socket_.async_read_some(boost::asio::buffer(recv_buffer, ws_framedata::BUFFER_SIZE),
							std::bind(&ws_client::ws_handshake_read_handle, self,
								std::placeholders::_1,
								std::placeholders::_2));
						return;
					}
				}
			}

			const char* bad_request_400 = "HTTP/1.1 400 Bad Request\r\n\r\n";
			boost::asio::async_write(this->socket_, boost::asio::buffer(bad_request_400, strlen(bad_request_400)),
				[this, self](const boost::system::error_code& error, size_t bytes_sended)
			{
				___lock___(this->recv_lock_, "handshake");
				this->on_error(dooqu_service::net::service_error::WS_HANDSHAKE_ERROR);
			});

		}

		bool ws_client::confirm_ws_handshake_and_response(ws_request& req)
		{
			bool upgrade_header = false, connect_header = false;
			const char* key = NULL, key1 = NULL, key2 = NULL;

			for (int i = 0, j = req.headers.size(); i < j; i++)
			{
				if (_stricmp(req.headers.at(i).name.c_str(), "Sec-WebSocket-Key") == 0)
				{
					key = req.headers.at(i).value.c_str();
				}
				if (upgrade_header == false && _stricmp(req.headers.at(i).name.c_str(), "Upgrade") == 0 && _stricmp(req.headers.at(i).value.c_str(), "websocket") == 0)
				{
					upgrade_header = true;
				}
				if (connect_header == false && _stricmp(req.headers.at(i).name.c_str(), "Connection") == 0 && _stricmp(req.headers.at(i).value.c_str(), "Upgrade") == 0)
				{
					connect_header = true;
				}
				if (connect_header && upgrade_header)
				{
					if (key != NULL)
					{
						goto _label_mode_1;
					}
					else if (key1 != NULL &&key2 != NULL)
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
				char key_data[128] = { 0 };
				sprintf(key_data, "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);

				char accept_key[29] = { 0 };
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


		void ws_client::read_from_client()
		{
			this->socket_.async_read_some(boost::asio::buffer(this->frame_data_.data, ws_framedata::BUFFER_SIZE),
				std::bind(&ws_client::on_data_received, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
		}

		void ws_client::on_data_received(const boost::system::error_code& error, size_t bytes_received)
		{
			if (!error)
			{
				do
				{
					___lock___(this->recv_lock_, "ws_client::on_data_received");
					framedata_parse_result result = data_parser_.parse(frame_data_, bytes_received);
					if (result == framedata_ok)
					{
						this->on_frame_data(&frame_data_);

						if (this->available() == false)
						{
							this->on_error(this->error_code_);
							return;
						}
						//粘包，继续解析
						if (frame_data_.pos_ < frame_data_.length)
						{
							continue;
						}
						//整包处理完毕，重置，继续读取
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
					std::bind(&ws_client::on_data_received, shared_from_this(),
						std::placeholders::_1, std::placeholders::_2));
			}
			else
			{
				___lock___(this->recv_lock_, "ws_client::on_data_received");
				this->on_error(dooqu_service::net::service_error::CLIENT_NET_ERROR);
			}
		}

		//注意，send_buffer_sequence默认构造了一定的量，正常情况下是够用的，如果不够用，最大可创建
		//MAX_BUFFER_SEQUENCE_SIZE个，但MAX_BUFFER_SEQUENCE_SIZE-8个，要引发复制构造函数，效率低下
		bool ws_client::alloc_available_buffer(buffer_stream** buffer_alloc)
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

				buffer_stream* curr_buffer = buffer_stream::create(MAX_BUFFER_SIZE);
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


		void ws_client::write(char* data)
		{
			//this->write(data);
			this->write("%s", data);
		}


		void ws_client::write(const char* format, ...)
		{
			buffer_stream* curr_buffer = NULL;

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
					std::bind(&ws_client::send_handle, shared_from_this(), std::placeholders::_1));
				//this->is_data_sending_ = true;
			}
		}


		void ws_client::write_frame(bool isFin, ws_framedata::opcode opcode, const char* format, ...)
		{
			buffer_stream* curr_buffer = NULL;

			___lock___(this->send_lock_, "ssl_connection::write::send_buffer_lock_");
			if (this->alloc_available_buffer(&curr_buffer) == false)
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
			//std::cout << "缓冲区为" << curr_buffer->read() << std::endl;
			if (read_pos_ == -1)
			{
				//只要read_pos_ == -1，说明write没有在处理任何数据，说明没有处于发送状态
				++read_pos_;
				boost::asio::async_write(this->socket_, boost::asio::buffer(curr_buffer->read() + curr_buffer->pos_start, curr_buffer->size()),
					std::bind(&ws_client::send_handle, shared_from_this(), std::placeholders::_1));
				//this->is_data_sending_ = true;
			}
		}


		///在发送完毕后，对发送队列中的数据进行处理；
		///发送数据是靠async_send 来实现的， 它的内部是使用多次async_send_same来实现数据全部发送；
		///所以，如果想保障数据的有序、正确到达，一定不要在第一次async_send结束之前发起第二次async_send；
		///实现采用一个发送的数据报队列，按数据报进行依次的发送,靠回调来驱动循环发送，直到当次队列中的数据全部发送完毕；
		void ws_client::send_handle(const boost::system::error_code& error)
		{
			___lock___(this->send_lock_, "ssl_connection::send_handle::send_buffer_lock_");
			if (error)
			{
				read_pos_ = -1;
				write_pos_ = 0;
				//this->is_data_sending_ = false;
				return;
			}

			buffer_stream* curr_buffer = NULL;
			if (++read_pos_ >= write_pos_)
			{
				read_pos_ = -1;
				write_pos_ = 0;
				//this->is_data_sending_ = false;
				return;
			}
			else
			{
				buffer_stream* curr_buffer = this->send_buffer_sequence_.at(this->read_pos_);
				boost::asio::async_write(this->socket_, boost::asio::buffer(curr_buffer->read() + curr_buffer->pos_start, curr_buffer->size()),
					std::bind(&ws_client::send_handle, shared_from_this(), std::placeholders::_1));
			}
		}


		ws_client::~ws_client()
		{
			std::cout << "~ws_client" << std::endl;
			//boost::system::error_code err_code;
			{
				//___lock___(this->send_buffer_lock_, "ssl_connection::~ssl_connection::send_buffer_lock");
				int size = this->send_buffer_sequence_.size();

				for (int i = 0; i < size; i++)
				{
					buffer_stream* curr_buffer = this->send_buffer_sequence_.at(i);
					buffer_stream::destroy(curr_buffer);
				}
				this->send_buffer_sequence_.clear();
			}
		}
	}
}



