#include "data_handler.h"

namespace dooqu_service
{
namespace service
{

data_handler::~data_handler()
{
}


void data_handler::dispatch_data(ws_client* client, char* data)
{
	this->on_client_data(client, data, strlen(data));
}


int data_handler::on_client_handshake(ws_client*, ws_request* req)
{
	return service_error::NO_ERROR;
}


void data_handler::on_client_framedata(ws_client* client, dooqu_service::basic::ws_framedata* framedata)
{
	switch (framedata->opcode_)
	{
	case ws_framedata::CONTINUATION:
	case ws_framedata::TEXT:
	case ws_framedata::BINARY:
		this->on_client_data(client, &framedata->data[framedata->data_pos_], framedata->payload_length_);
		break;
	case ws_framedata::PING:
	case ws_framedata::PONG:
		break;
	case ws_framedata::CLOSE:
		break;
	default:
		client->disconnect(service_error::WS_ERROR_UNSUPPORTED_DATA_TYPE, "UNSUPPORTED_DATE_TYPE");
		break;
	}
}

void data_handler::on_client_data(ws_client* client, char* payload_data, unsigned long long  payload_size)
{
}

}
}
