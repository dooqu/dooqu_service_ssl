#ifndef __NET_SERVICE_ERROR_H__
#define __NET_SERVICE_ERROR_H__


namespace dooqu_service
{
	namespace net
	{
#undef NO_ERROR
		struct service_error
		{
			static const int NO_ERROR = 0;
			static const int CLIENT_NET_ERROR = 1;	
			static const int DATA_OUT_OF_BUFFER = 2;
			static const int SERVER_CLOSEING = 3;
			static const int CLIENT_DATA_SEND_BLOCK = 4;
			static const int SSL_HANDSHAKE_ERROR = 5;
			static const int WS_HANDSHAKE_ERROR = 6;
		};
	}
}

#endif
