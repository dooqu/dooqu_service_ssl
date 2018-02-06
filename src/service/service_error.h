#ifndef __SERVICE_SERVICE_ERROR_H__
#define __SERVICE_SERVICE_ERROR_H__

#include "../net/service_error.h"

namespace dooqu_service
{
namespace service
{
struct service_error : dooqu_service::net::service_error
{
	static const unsigned short TIME_OUT = 4016;
    static const unsigned short CLIENT_HAS_LOGINED = 4011;
    static const unsigned short GAME_IS_FULL = 4012;
    static const unsigned short GAME_NOT_EXISTED = 4015;   
    static const unsigned short SERVER_CLOSEING = 1001;
    static const unsigned short ARGUMENT_ERROR = 4018;
    static const unsigned short LOGIN_SERVICE_ERROR = 4019;
    static const unsigned short LOGIN_CONCURENCY_LIMIT = 4020;
    static const unsigned short CONSTANT_REQUEST = 4021;
    static const unsigned short COMMAND_ERROR = 4022;

};
}
}

#endif
