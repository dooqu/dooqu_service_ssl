#ifndef __GAME_CLIENT_H__
#define __GAME_CLIENT_H__

#include <iostream>
#include <mutex>
#include <codecvt>
#include "../net/ws_session.h"
#include "game_info.h"
#include "command.h"
#include "command_dispatcher.h"
#include "../util/tick_count.h"
#include "post_monitor.h"

using namespace dooqu_service::net;
using namespace boost::asio;

namespace dooqu_service
{
namespace service
{
class game_plugin;

template<class SOCK_TYPE>
class game_service;

template<class SOCK_TYPE>
class game_session : public ws_session<SOCK_TYPE>
{
	friend class game_plugin;
	friend class command_dispatcher;
	friend class game_service<SOCK_TYPE>;

	typedef std::shared_ptr<game_session<SOCK_TYPE>> game_session_ptr;
private:
	void simulate_command_process(char *command_data)
	{
		if (this->command_dispatcher_ != NULL)
		{
			this->command_dispatcher_->simulate_client_data(this, command_data);
		}
		//delete [] command_data;
		free(command_data);
	}

  char *update_url_;
protected:
	enum { ID_LEN = 16, NAME_LEN = 32, UP_RE_TIMES = 3 };
	game_info* game_info_;
	char id_[ID_LEN];
	char name_[NAME_LEN];
	command_dispatcher* command_dispatcher_;
	command command_;
	tick_count actived_time;
	post_monitor message_monitor_;
	post_monitor active_monitor_;
	int update_retry_count_;
	int plugin_addr_;

	std::thread::id* curr_dispatcher_thread_id_;

	virtual int on_ws_handshake(ws_request* request)
	{
		if (this->command_dispatcher_ != NULL)
		{
			return this->command_dispatcher_->on_client_handshake(this, request);
		}
		return 0;
	}

	virtual void on_error(const int error)
	{
		assert(this->command_dispatcher_ != NULL);
		if (this->is_error_ == false)
		{
			this->is_error_ = true;
			if (this->get_error_code() == service_error::NO_ERROR)
			{
				this->set_error_code(error);
			}
			if (this->command_dispatcher_ != NULL)
			{
				this->ios.post(std::bind(&command_dispatcher::dispatch_bye, this->command_dispatcher_, this));
			}
		}
	}

	void fill(char* id, char* name, char* profile)
	{
		int id_len = std::strlen(id);
		int name_len = std::strlen(name);
		int pro_len = (profile == NULL) ? 0 : std::strlen(profile);

		int min_id_len = std::min((ID_LEN - 1), id_len);
		int min_name_len = std::min((NAME_LEN - 1), name_len);

		strncpy(this->id_, id, min_id_len);
		strncpy(this->name_, name, min_name_len);

		this->id_[min_id_len] = 0;
		this->name_[min_name_len] = 0;
	}

  public:
	game_session(io_service& ios, ssl::context& context);
	game_session(io_service& ios);
	virtual ~game_session()
	{
	}

	char* id()
	{
		return this->id_;
	}

	char* name()
	{
		return this->name_;
	}

	void dispatch_data(char* command_data)
	{
		printf("dispatch_data:%s\n", command_data);

		if (this->command_dispatcher_ == NULL)
			return;

		int command_data_len = strlen(command_data);

		assert((command_data_len + 1) <= game_session<SOCK_TYPE>::MAX_BUFFER_SIZE);

		bool locked = this->recv_lock_.try_lock();
		if (locked && this->curr_dispatcher_thread_id_ == &std::this_thread::get_id())
		{
			char command_data_clone[ws_session<SOCK_TYPE>::MAX_BUFFER_SIZE];
			command_data_clone[sprintf(command_data_clone, command_data, command_data_len)] = 0;
			this->command_dispatcher_->simulate_client_data(this, command_data_clone);
			this->recv_lock_.unlock();
		}
		else
		{
			assert(this->curr_dispatcher_thread_id_ != &std::this_thread::get_id());

			char *command_data_clone = (char *)malloc(command_data_len + 1); //new char[command_data_len + 1];
			std::strcpy(command_data_clone, command_data);
			command_data_clone[std::strlen(command_data)] = 0;
			this->recv_lock_.unlock();
			this->ios.post(std::bind(&game_session<SOCK_TYPE>::simulate_command_process, this, command_data_clone));
		}
	}

	void set_command_dispatcher(void* dispatcher)
	{
		command_dispatcher* cmd_dispatcher = (command_dispatcher*)dispatcher;
		this->command_dispatcher_ = cmd_dispatcher;	
	}

	void active()
	{
		this->actived_time.restart();
	}

	int64_t actived_time_elapsed()
	{
		return this->actived_time.elapsed();
	}

	int update_retry_count(bool increase = false)
	{
		return (increase)? ++update_retry_count_ : update_retry_count_;
	}

	void* get_command()
	{
		return &this->command_;
	}

	void* get_game_info()
	{
		return this->game_info_;
	}

	template<typename T>
	T* get_game_info()
	{
		return (T*)this->game_info_;
	}

	void set_game_info(void* info)
	{
		this->game_info_ = (game_info*)info;
	}

	bool can_message()
	{
		return this->message_monitor_.can_active();
	}

	bool can_active()
	{
		return this->active_monitor_.can_active();
	}

	virtual void on_frame_data(ws_framedata* frame)
	{
		//std::cout << "RECV:" << &frame->data[frame->data_pos_] << std::endl; 
		if (this->command_dispatcher_ != NULL)
		{
			this->command_dispatcher_->on_client_framedata(this, frame);
		}
	}

	virtual std::shared_ptr<game_session<SOCK_TYPE>> get_shared_ptr()
	{
		return std::dynamic_pointer_cast<game_session<SOCK_TYPE>>(shared_from_this());
	}

	void disconnect(unsigned short code, char *reason)
	{
		___lock___(this->recv_lock_, "game_client::disconnect_int.recv_lock_");
		// if (this->available())
		this->available_ = false;
		this->error_code_ = code;
		this->write_error(code, reason);
	}
};

///////////////////
template<>
inline game_session<tcp_stream>::game_session(io_service& ios) :
	ws_session<tcp_stream>(ios),
	game_info_(NULL),
	command_dispatcher_(NULL),
	plugin_addr_(0),
	curr_dispatcher_thread_id_(NULL)
{
	this->id_[0] = 0;
	this->name_[0] = 0;
	this->update_retry_count_ = 0;
	this->active();
	this->message_monitor_.init(5, 4000);
	this->active_monitor_.init(30, 60 * 1000);
}



template<>
inline game_session<ssl_stream>::game_session(io_service& ios, boost::asio::ssl::context& context) :
	ws_session<ssl_stream>(ios, context),
	game_info_(NULL),
	command_dispatcher_(NULL),
	plugin_addr_(0),
	curr_dispatcher_thread_id_(NULL)
{
	this->id_[0] = 0;
	this->name_[0] = 0;
	this->update_retry_count_ = 0;
	this->active();
	this->message_monitor_.init(5, 4000);
	this->active_monitor_.init(30, 60 * 1000);
}


//on_error的主要功能是将用户的离开和逻辑错误动作传递给command_dispatcher对象进行依次处理。
//error_code_的初始默认值为CLIENT_NET_ERROR；
//如果这个值被改动，说明在on_error之前、调用过disconnect、并传递过断开的原因；
//这样在tcp_client的断开处理中、即使传递0，也不会被赋值；
/*
template<class SOCK_TYPE>
void game_session<SOCK_TYPE>::on_error(const int error)
{
	assert(this->command_dispatcher_ != NULL);
	if (this->is_error_ == false)
	{
		this->is_error_ = true;

		if (this->error_code_ == service_error::NO_ERROR)
		{
			this->error_code_ = error;
		}
		if (this->command_dispatcher_ != NULL)
		{
			this->ios.post(std::bind(&command_dispatcher::dispatch_bye, this->command_dispatcher_, this));
		}
	}
}


template<class SOCK_TYPE>
void game_session<SOCK_TYPE>::disconnect(unsigned short code, char* reason)
{
	___lock___(this->recv_lock_, "game_client::disconnect_int.recv_lock_");
	// if (this->available())
	{
		this->available_ = false;
		this->error_code_ = code;
		this->write_error(code, "reason is");
	}
}
*/


}
}

#endif
