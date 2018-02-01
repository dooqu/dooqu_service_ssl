#include "game_client.h"

namespace dooqu_service
{
namespace service
{
game_client::game_client(io_service& ios, boost::asio::ssl::context& context) :
	ws_client(ios, context),
    game_info_(NULL),
	command_dispatcher_(NULL),
    plugin_addr_(0),
    curr_dispatcher_thread_id_(NULL)
{
    this->id_[0] = 0;
    this->name_[0] = 0;
    this->retry_update_times_ = UP_RE_TIMES;
    this->active();
    this->message_monitor_.init(5, 4000);
    this->active_monitor_.init(30, 60 * 1000);
}


game_client::~game_client()
{
}

//on_error的主要功能是将用户的离开和逻辑错误动作传递给command_dispatcher对象进行依次处理。
//error_code_的初始默认值为CLIENT_NET_ERROR；
//如果这个值被改动，说明在on_error之前、调用过disconnect、并传递过断开的原因；
//这样在tcp_client的断开处理中、即使传递0，也不会被赋值；
void game_client::on_error(const int error)
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


void game_client::fill(char* id, char* name, char* profile)
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


void game_client::dispatch_data(char* command_data)
{
    printf("dispatch_data:%s\n", command_data);

    if (this->command_dispatcher_ == NULL)
        return;

    int command_data_len = strlen(command_data);

    assert((command_data_len + 1) <= game_client::MAX_BUFFER_SIZE);

    bool locked = this->recv_lock_.try_lock();
    if(locked && this->curr_dispatcher_thread_id_ == &std::this_thread::get_id())
    {
        char command_data_clone[ws_client::MAX_BUFFER_SIZE];
        command_data_clone[sprintf(command_data_clone, command_data, command_data_len)] = 0;
        this->command_dispatcher_->simulate_client_data(this, command_data_clone);
        this->recv_lock_.unlock();
    }
    else
    {
        assert(this->curr_dispatcher_thread_id_ != &std::this_thread::get_id());

        char* command_data_clone = (char*)malloc(command_data_len + 1);//new char[command_data_len + 1];
        std::strcpy(command_data_clone, command_data);
        command_data_clone[std::strlen(command_data)] = 0;
        this->recv_lock_.unlock();
        this->ios.post(std::bind(&game_client::simulate_command_process, this, command_data_clone));
    }
}

void game_client::simulate_command_process(char* command_data)
{
    if(this->command_dispatcher_ != NULL)
    {
        this->command_dispatcher_->simulate_client_data(this, command_data);
    }
    //delete [] command_data;
    free(command_data);
}

void game_client::disconnect(int code)
{
    ___lock___(this->recv_lock_, "game_client::disconnect_int.recv_lock_");
   // if (this->available())
    {
		this->available_ = false;
        this->error_code_ = code;
		this->write_error(code, "reason is");
    }
}

}
}
