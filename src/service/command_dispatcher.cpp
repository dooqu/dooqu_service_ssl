#include "command_dispatcher.h"

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


void command_dispatcher::simulate_client_data(ws_client* client, char* data)
{
	___lock___(client->get_recv_mutex(), "command_dispatcher::simulate_client_data");
	this->on_client_data(client, data);
}


int command_dispatcher::on_client_handshake(ws_client*, ws_request* req)
{
	return 0;
}


void command_dispatcher::on_client_framedata(ws_client* client, dooqu_service::basic::ws_framedata* framedata)
{
	//std::string rep = ws_util::encode_to_utf8(L"我收到你的消息");
	switch (framedata->opcode_)
	{
	case ws_framedata::CONTINUATION:
	case ws_framedata::TEXT:
	case ws_framedata::BINARY:
		this->on_client_data(client, &framedata->data[framedata->data_pos_]);
		ws_util::wprint(framedata->data_begin(), framedata->data_end());
		break;
	case ws_framedata::PING:
	case ws_framedata::PONG:
		break;
	case ws_framedata::CLOSE:
		break;
	default:
		//不合规的数据，关闭客户链接
		client->disconnect(service_error::WS_ERROR_UNSUPPORTED_DATA_TYPE, "error data type.");
		break;
	}
}

void command_dispatcher::on_client_data(ws_client* client, char* frame_data)
{
	// client->curr_dispatcher_thread_id_ = &std::this_thread::get_id();
	command* client_command = ((command*)client->get_command());
	client_command->reset(frame_data);
	if (client_command->is_correct())
	{
		this->on_client_command(client, client_command);
	}
	// client->curr_dispatcher_thread_id_ = NULL;
}


void command_dispatcher::on_client_command(ws_client* client, command* command)
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
