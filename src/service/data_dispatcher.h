#ifndef DATA_DISPATHCER_H
#define DATA_DISPATHCER_H

#include <map>
#include <cstring>
#include "command.h"
#include "service_error.h"
#include "../net/ws_request.h"
#include "../net/ws_framedata_parser.h"
#include "../util/utility.h"
#include "../util/ws_util.h"
#include "../util/char_key_op.h"
#include "game_client.h"

namespace dooqu_service
{
namespace service
{
#undef NO_ERROR;
	class data_dispacher
	{
	protected:
		virtual void dispatch_bye(game_client*) = 0;
		virtual int on_client_handshake(game_client*, ws_request*)
		{
			return dooqu_service::net::service_error::NO_ERROR;
		}
		virtual void on_client_framedata(game_client*, ws_framedata*) = 0;
	};
}
}

#endif
