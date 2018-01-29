#include "command_dispatcher.h"
#include "game_client.h"

namespace dooqu_service
{
namespace service
{
command_dispatcher::~command_dispatcher()
{
    this->unregist_all_handles();
}


bool command_dispatcher::regist_handle(const char* cmd_name, command_handler handler)
{
    std::map<const char*, command_handler, char_key_op>::iterator handle_pair = this->handles.find(cmd_name);

    if (handle_pair == this->handles.end())
    {
        this->handles[cmd_name] = handler;
        return true;
    }
    else
    {
        return false;
    }
}


void command_dispatcher::simulate_client_data(game_client* client, char* data)
{
	___lock___(client->recv_lock_, "command_dispatcher::simulate_client_data");
    this->on_client_data(client, data);
}

int command_dispatcher::on_client_handshake(game_client*, ws_request* req)
{
	return 0;
}

void command_dispatcher::on_client_framedata(game_client* client, dooqu_service::net::ws_framedata* framedata)
{
	//std::string rep = ws_util::encode_to_utf8(L"我收到你的消息");
	switch (framedata->opcode_)
	{
	case ws_framedata::CONTINUATION:
	case ws_framedata::TEXT:
	case ws_framedata::BINARY:
		this->on_client_data(client, &framedata->data[framedata->data_pos_]);
		break;
	case ws_framedata::PING:
	case ws_framedata::PONG:
		break;
	case ws_framedata::CLOSE:
		client->write_frame(true, dooqu_service::net::ws_framedata::opcode::CLOSE, "");
		break;
	default:
		break;
	}
}

void command_dispatcher::on_client_data(game_client* client, char* frame_data)
{
	if (client->available_ == false)
	{
		return;
	}
	client->curr_dispatcher_thread_id_ = &std::this_thread::get_id();
	client->command_.reset(frame_data);
	if (client->command_.is_correct())
	{
		this->on_client_command(client, &client->command_);
	}
	client->curr_dispatcher_thread_id_ = NULL;
}


void command_dispatcher::on_client_command(game_client* client, command* command)
{
    std::map<const char*, command_handler, char_key_op>::iterator handle_pair = this->handles.find(command->name());

    if (handle_pair != this->handles.end())
    {
        command_handler* handle = &handle_pair->second;
        (this->**handle)(client, command);
    }
}


void command_dispatcher::unregist_handle(char* cmd_name)
{
    this->handles.erase(cmd_name);
}


void command_dispatcher::unregist_all_handles()
{
    this->handles.clear();
}
}
}
