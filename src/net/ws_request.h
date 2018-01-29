#ifndef WS_REQUEST_H
#define WS_REQUEST_H

#include <string>
#include <vector>

namespace dooqu_service
{
namespace net
{

struct header
{
    std::string name;
    std::string value;
};

struct ws_request
{
    std::string method;
    std::string uri;
    int http_version_major;
    int http_version_minor;
    std::vector<header> headers;
	unsigned int content_size;

	ws_request() : content_size(0)
	{
	}
};

}
}

#endif // WS_REQUEST_H
