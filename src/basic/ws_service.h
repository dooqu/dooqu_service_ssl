#ifndef __WS_SERVICE_H__
#define __WS_SERVICE_H__
#include <functional>
#include <cstring>
#include "boost/system/error_code.hpp"
#include "boost/asio/io_service.hpp"
namespace dooqu_service
{
namespace basic
{
class ws_client;
typedef std::function<void(const boost::system::error_code&, const int status_code, const std::string&)> http_request_callback;

class ws_service
{
public:
virtual void post_handle(std::function<void(void)> handle) = 0;
virtual void post_handle(std:: function<void(void)> handle, int millisecs_delay, bool cancel_enable) = 0;
virtual bool start_http_request(const char* host, const char* path, http_request_callback callback) = 0;
virtual bool queue_http_request(const char* host, const char* path, http_request_callback callback) = 0;
virtual boost::asio::io_service& get_io_service() = 0;
virtual bool is_running() = 0;
};
}
}

#endif