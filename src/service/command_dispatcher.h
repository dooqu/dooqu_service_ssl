#ifndef __COMMAND_DISPATCHER_H__
#define __COMMAND_DISPATCHER_H__

#include <map>
#include <cstring>
#include "command.h"
#include "service_error.h"
#include "../basic/ws_client.h"
#include "../basic/ws_framedata.h"
#include "../net/ws_request.h"
#include "../util/utility.h"
#include "../util/ws_util.h"
#include "../util/char_key_op.h"

namespace dooqu_service
{
namespace service
{
using namespace dooqu_service::util;
using namespace dooqu_service::net;
using namespace dooqu_service::basic;
//预先先game_client声明在这， 先不引用game_client.h，容易造成交叉引用


#define make_handler(_handler) (command_dispatcher::command_handler)(&_handler)

class command_dispatcher
{
    friend class game_plugin;
public:
    typedef void (command_dispatcher::*command_handler)(ws_client* client, command* cmd);
    virtual ~command_dispatcher();

public:
    std::map<const char*, command_handler, char_key_op> handles;

    bool regist_handle(const char* cmd_name, command_handler handler);
    void unregist_handle(char* cmd_name);
    void unregist_all_handles();
    virtual void simulate_client_data(ws_client*, char* data);
    virtual void dispatch_bye(ws_client*) = 0;

	virtual int on_client_handshake(ws_client*, ws_request*);
    virtual void on_client_framedata(ws_client*, ws_framedata* );
    virtual void on_client_command(ws_client*, command* command);
	virtual void on_client_data(ws_client*, char* frame_data);

private:
};
}
}

#endif
