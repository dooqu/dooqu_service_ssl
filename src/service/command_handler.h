#ifndef __COMMAND_DISPATCHER_H__
#define __COMMAND_DISPATCHER_H__

#include <map>
#include <cstring>

//#include "service_error.h"
//#include "../basic/ws_client.h"
//#include "../basic/ws_framedata.h"
//#include "../net/ws_request.h"
#include "../util/utility.h"
#include "../util/ws_util.h"
#include "../util/char_key_op.h"
#include "data_handler.h"
#include "command.h"

namespace dooqu_service
{
namespace service
{
using namespace dooqu_service::util;
using namespace dooqu_service::net;
using namespace dooqu_service::basic;

#define make_handler(_handler) (typename command_handler::command_handler_callback)(&_handler)

class command_handler : public dooqu_service::service::data_handler
{    
    friend class game_plugin;
public:
    typedef void (command_handler::*command_handler_callback)(ws_client* client, command* cmd);
    virtual ~command_handler();
public:
    std::map<const char*, command_handler_callback, char_key_op> handles;

    bool regist_handle(const char* cmd_name, command_handler_callback handler);
    void unregist_handle(char* cmd_name);
    void unregist_all_handles();

    void dispatch_data(ws_client*, char* data);
    void on_client_command(ws_client*, command* command);
	void on_client_data(ws_client*, char* frame_data, unsigned long long payload_size);

private:
};


}
}

#endif
