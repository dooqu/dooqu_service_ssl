#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

#include <cstdarg>
#include <string>
#include <iostream>
#include <mutex>
#include <functional>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/pool/singleton_pool.hpp>
#include <boost/noncopyable.hpp>
#include "service_error.h"
#include "ws_request_parser.h"
#include "ws_framedata_parser.h"
#include "../util/ws_util.h"
#include "../util/utility.h"
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif
#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

namespace dooqu_service
{
namespace net
{

using namespace boost::asio;
using namespace boost::asio::ip;


class buffer_stream : boost::noncopyable
{
protected:
    std::vector<char> buffer_;
    int size_;
    enum
    {
        DATA_POS = 4
    };

public:
    int pos_start;
    static buffer_stream* create(int buffer_size)
    {
        assert(buffer_size > DATA_POS);
        void* buffer_chunk = memory_pool_malloc<buffer_stream>();
        if(buffer_chunk != NULL)
        {
            buffer_stream* bs = new(buffer_chunk) buffer_stream(buffer_size);
            return bs;
        }
        return NULL;
    }

    static void destroy(buffer_stream* bs)
    {
        if(bs != NULL)
        {
            bs->~buffer_stream();
            memory_pool_free<buffer_stream>(bs);
        }
    }

    char* read(int pos = 0)
    {
        //跳过fin一个字节， opcode一个字节， 最大长度4个字节
        return &*this->buffer_.begin() + pos;
    }

    //设定buffer的默认size 和默认值
    buffer_stream(size_t size) : buffer_(size, 0)
    {
        this->size_ = 0;
    }

    virtual ~buffer_stream()
    {
        this->buffer_.clear();
    }

    int size()
    {
        return this->size_;
    }


    int write(const char* format, va_list arg_ptr)
    {
        int bytes_readed = vsnprintf(read(), this->buffer_.size(), format, arg_ptr);
		if (bytes_readed < 0)
		{
			return bytes_readed;
		}
        if (bytes_readed >= this->buffer_.size())
        {
            return 0;
        }

        this->size_ = bytes_readed;
		this->pos_start = 0;
        return bytes_readed;
    }
    //向buffer_stream中写入数据
    int write_frame(const char* format, va_list arg_ptr)
    {
        //写入的位置是向后移动DATA_POS个位置，留出空位，写入fin和opcode；
        //可写入的长度是
        int bytes_readed = vsnprintf(read() + DATA_POS, this->buffer_.size() - DATA_POS, format, arg_ptr);
		if (bytes_readed < 0)
		{
			return bytes_readed;
		}
        if (bytes_readed >= (this->buffer_.size() - DATA_POS))
        {
            return 0;
        }

        if(bytes_readed < 126)
        {
            *at(3) = bytes_readed;
            pos_start = 2;
        }
        else if(bytes_readed < 65536)
        {
            bytes_readed = htons(bytes_readed);
            memcpy(at(2), &bytes_readed, 2);
            pos_start = 0;
        }
        else
        {
            return -1;
        }
        this->size_ = bytes_readed + DATA_POS - pos_start;
        return this->size_;
    }

    int capacity()
    {
        return this->buffer_.capacity();
    }

    void double_size()
    {
        this->buffer_.resize(this->buffer_.size() * 2, 0);
    }

    char* at(int pos)
    {
        return &this->buffer_.at(pos);
    }
};

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;

class ws_client : boost::noncopyable
{
public:
    enum
    {
        MAX_BUFFER_SIZE = 128,
        MAX_BUFFER_SEQUENCE_SIZE = 32,
        MAX_BUFFER_SIZE_DOUBLE_TIMES = 4
    };

protected:
    bool is_data_sending_;

    //发送数据缓冲区
    std::vector<buffer_stream*> send_buffer_sequence_;

    //数据锁
    std::recursive_mutex send_lock_;

	std::recursive_mutex recv_lock_;

    //正在处理的位置，这个位置是发送函数正在读取的位置
    int read_pos_;

    //write_pos_永远指向该写入的位置
    int write_pos_;

    int error_code_;

	bool is_error_;

    //接收数据缓冲区
    //char buffer[MAX_BUFFER_SIZE];
    char* recv_buffer;
    //指向缓冲区的指针，使用asio::buffer会丢失位置
    //char* p_buffer;

    //当前缓冲区的位置
    unsigned int buffer_pos;

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
        return this->available_ ;
    }

    void write_frame(bool isFin, ws_framedata::opcode, const char* format, ...);

    void write(char* data);

    void write(const char* format, ...);

};

}
}


#endif
