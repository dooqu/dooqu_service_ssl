#ifndef __GAME_CLIENT_H__
#define __GAME_CLIENT_H__

#include <iostream>
#include <mutex>
#include <codecvt>
#include "../net/ws_client.h"
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
class game_client : public ws_client
{
    friend class game_service;
    friend class game_plugin;
    friend class command_dispatcher;
private:
    void simulate_command_process(char* command_data);
    char* update_url_;
protected:
    enum {ID_LEN = 16, NAME_LEN = 32, UP_RE_TIMES = 3};
    game_info* game_info_;
    char id_[ID_LEN];
    char name_[NAME_LEN];
    command_dispatcher* command_dispatcher_;
	command command_;
    tick_count actived_time;
    post_monitor message_monitor_;
    post_monitor active_monitor_;
    int retry_update_times_;
    int plugin_addr_;
	
    std::thread::id* curr_dispatcher_thread_id_;

	virtual int on_ws_handshake(ws_request* request)
	{
		if (this->command_dispatcher_ != NULL)
		{
			//___lock___(this->commander_mutex_, "command_dispatcher::on_client_ws_handshake");
			return this->command_dispatcher_->on_client_handshake(this, request);
		}
		return 0;
	}

    virtual void on_error(const int error);

    void fill(char* id, char* name, char* profile);
public:
    game_client(io_service& ios, ssl::context& context);
    virtual ~game_client();

    char* id()
    {
        return this->id_;
    }
    char* name()
    {
        return this->name_;
    }

    void dispatch_data(char* command_data);

    void set_command_dispatcher(command_dispatcher* dispatcher)
    {
        this->command_dispatcher_ = dispatcher;
    }

    void active()
    {
        this->actived_time.restart();
    }

    long long get_actived()
    {
        return this->actived_time.elapsed();
    }

    game_info* get_game_info()
    {
        return this->game_info_;
    }

    template<typename T>
    T* get_game_info()
    {
        return (T*)this->game_info_;
    }

    void set_game_info(game_info* info)
    {
        this->game_info_ = info;
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
        std::cout << "on_frame_data" << std::endl;
		if (this->command_dispatcher_ != NULL)
		{
			if (this->available())
			{
				this->command_dispatcher_->on_client_framedata(this, frame);
			}			
		}
    }

	std::shared_ptr<game_client> shared_from_self()
	{
		return std::dynamic_pointer_cast<game_client>(ws_client::shared_from_this());
	}

    void disconnect(int code);
    //void disconnect();
};

typedef std::shared_ptr<game_client> game_client_ptr;
}
}

#endif
