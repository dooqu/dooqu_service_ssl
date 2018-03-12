#include "command_handler.h"

namespace dooqu_service
{
namespace service
{
command_handler::~command_handler()
{
	this->unregist_all_handles();
}


bool command_handler::regist_handle(const char* cmd_name, command_handler_callback handler)
{
	std::map<const char*, command_handler_callback, char_key_op>::iterator handle_pair = this->handles.find(cmd_name);

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


void command_handler::dispatch_data(ws_client* client, char* data)
{
	this->on_client_data(client, data, strlen(data) + 1);
}


void command_handler::on_client_data(ws_client* client, char* frame_data, unsigned long long payload_size)
{
	command client_command;
	client_command.reset(frame_data);
	if (client_command.is_correct())
	{
		this->on_client_command(client, &client_command);
	}
}


void command_handler::on_client_command(ws_client* client, command* command)
{
	std::map<const char*, command_handler_callback, char_key_op>::iterator handle_pair = this->handles.find(command->name());

	if (handle_pair != this->handles.end())
	{
		command_handler_callback* handle = &handle_pair->second;
		(this->**handle)(client, command);
	}
}


void command_handler::unregist_handle(char* cmd_name)
{
	this->handles.erase(cmd_name);
}


void command_handler::unregist_all_handles()
{
	this->handles.clear();
}
}
}
