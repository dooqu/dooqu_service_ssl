#ifndef __SERVICE_SERVICE_ERROR_H__
#define __SERVICE_SERVICE_ERROR_H__

#include "../net/service_error.h"

namespace dooqu_service
{
namespace service
{
struct service_error : dooqu_service::net::service_error
{
	static const int TIME_OUT = 4016;
    static const int CLIENT_HAS_LOGINED = 4011;
    static const int GAME_IS_FULL = 4012;
    static const int GAME_NOT_EXISTED = 4015;   
    static const int SERVER_CLOSEING = 1001;
    static const int ARGUMENT_ERROR = 4018;
    static const int LOGIN_SERVICE_ERROR = 4019;
    static const int LOGIN_CONCURENCY_LIMIT = 4020;
    static const int CONSTANT_REQUEST = 4021;
    static const int COMMAND_ERROR = 4022;

};
}
}

#endif
