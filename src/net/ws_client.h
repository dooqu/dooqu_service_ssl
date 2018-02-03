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
#include "service_error.h"
#include "ws_request_parser.h"
#include "ws_framedata_parser.h"
#include "../util/ws_util.h"
#include "buffer_stream.h"


namespace dooqu_service
{
namespace net
{

using namespace boost::asio;
using namespace boost::asio::ip;
typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;
class ws_client : public std::enable_shared_from_this<ws_client>, boost::noncopyable
{
public:
	enum
	{
		MAX_BUFFER_SIZE = 128,
		MAX_BUFFER_SEQUENCE_SIZE = 32,
		MAX_BUFFER_SIZE_DOUBLE_TIMES = 4
	};

protected:
	//发送数据缓冲区
	std::vector<buffer_stream*> send_buffer_sequence_;

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
	char* recv_buffer;

	//io_service对象
	io_service& ios;
	//包含的socket对象
	ssl_socket socket_;

	ws_request request_;

	ws_request_parser req_parser_;

	ws_framedata frame_data_;

	ws_framedata_parser data_parser_;

	//用来标识socket对象是否可用
	bool available_;

	void start();

	//开始从客户端读取数据
	void read_from_client();

	//用来接收数据的回调方法
	virtual void on_data_received(const boost::system::error_code& error, size_t bytes_received);

	//virtual void on_frame_data() ;
	void ws_handshake_read_handle(const boost::system::error_code& error, size_t bytes_received);


	bool confirm_ws_handshake_and_response(ws_request& req);

	//发送数据的回调方法
	void send_handle(const boost::system::error_code& error);
	//当有数据到达时会被调用，该方法为虚方法，由子类进行具体逻辑的处理和运用
	//inline virtual void on_data(char* client_data) = 0;
	//当客户端出现规则错误时进行调用， 该方法为虚方法，由子类进行具体的逻辑的实现。
	int error_code()
	{
		return this->error_code_;
	}

	virtual void on_error(const int error) = 0;


	virtual void on_frame_data(ws_framedata* framedata) = 0;

	virtual int on_ws_handshake(ws_request* request)
	{
		return 0;
	}


	virtual bool alloc_available_buffer(buffer_stream** buffer_alloc);

public:
	ws_client(io_service& ios, ssl::context& ssl_context);

	virtual ~ws_client();

	ssl_socket& socket()
	{
		return this->socket_;
	}


	inline bool available()
	{
		return this->available_;
	}

	void write_frame(bool isFin, ws_framedata::opcode, const char* format, ...);

	void write(char* data);

	void write(const char* format, ...);

	void write_error(uint16_t error_code, char* error_reason = NULL);

	void async_close();
};

typedef std::shared_ptr<ws_client> ws_client_ptr;

}
}


#endif
