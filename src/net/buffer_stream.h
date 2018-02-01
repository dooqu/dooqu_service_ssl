#ifndef __BUFFER_STREAM_H__
#define __BUFFER_STREAM_H__

#include <vector>
#include <boost/pool/singleton_pool.hpp>
#include <boost/noncopyable.hpp>
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

	class buffer_stream : boost::noncopyable
	{
	protected:
		std::vector<char> buffer_;
		bool is_error_frame_;
		int size_;
		enum
		{
			DATA_POS = 4
		};

	public:
		int pos_start;
		bool is_error_frame()
		{
			return this->is_error_frame_;
		}

		void set_error_frame(bool is_error_frame)
		{
			this->is_error_frame_ = is_error_frame;
		}
		
		static buffer_stream* create(int buffer_size)
		{
			assert(buffer_size > DATA_POS);
			void* buffer_chunk = memory_pool_malloc<buffer_stream>();
			if (buffer_chunk != NULL)
			{
				buffer_stream* bs = new(buffer_chunk) buffer_stream(buffer_size);
				return bs;
			}
			return NULL;
		}

		static void destroy(buffer_stream* bs)
		{
			if (bs != NULL)
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
		buffer_stream(size_t size) : buffer_(size, 0), is_error_frame_(false)
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

			if (bytes_readed < 126)
			{
				*at(3) = bytes_readed;
				pos_start = 2;
			}
			else if (bytes_readed < 65536)
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
}
}

#endif
