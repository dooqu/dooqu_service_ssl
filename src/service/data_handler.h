#ifndef __DOOQU_DATA_HANDLER_H__
#define __DOOQU_DATA_HANDLER_H__

#include <cstring>
#include "../basic/ws_client.h"
#include "../basic/ws_framedata.h"
#include "../net/ws_request.h"
#include "service_error.h"


namespace dooqu_service
{
namespace service
{

using namespace dooqu_service::basic;
using namespace dooqu_service::net;

class data_handler
{
    template<typename T> friend class game_session;
protected:
	virtual int on_client_handshake(ws_client*, ws_request*);
    virtual void on_client_framedata(ws_client*, ws_framedata* );
	virtual void on_client_data(ws_client*, char* payload_data, unsigned long long payload_size);
    virtual void dispatch_data(ws_client*, char* data);
    virtual void dispatch_bye(ws_client*) = 0;
public:
    virtual ~data_handler();

};
}
}

#endif
