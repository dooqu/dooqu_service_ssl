#ifndef __NET_SERVICE_ERROR_H__
#define __NET_SERVICE_ERROR_H__


namespace dooqu_service
{
	namespace net
	{
#undef NO_ERROR
		struct ws_error
		{			
			static const unsigned short WS_ERROR_NORMAL_CLOSURE = 1000;
			static const unsigned short WS_ERROR_GOING_AWAY = 1001;
			static const unsigned short WS_ERROR_PROTOCOL_ERROR = 1002;
			static const unsigned short WS_ERROR_UNSUPPORTED_DATA_TYPE = 1003;
			static const unsigned short WS_ERROR_INVALID_DATA = 1007;
			static const unsigned short WS_ERROR_POLICY_VIOLATION = 1008;
			static const unsigned short WS_ERROR_DATA_TOO_BIG = 1009;
			static const unsigned short WS_ERROR_INTERNAL_SERVER_ERROR = 1011;
		};

		struct service_error : ws_error
		{
			static const unsigned short NO_ERROR = 0;
			static const unsigned short DATA_OUT_OF_BUFFER = 1009;
			static const unsigned short SERVER_CLOSEING = 1001;
			static const unsigned short SSL_HANDSHAKE_ERROR = 1002;
			static const unsigned short WS_HANDSHAKE_ERROR = 1002;
		};
	}
}

#endif
