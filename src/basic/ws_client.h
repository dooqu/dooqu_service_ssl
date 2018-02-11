#ifndef __WS_CLIENT_H__
#define __WS_CLIENT_H__

#include <mutex>
#include "ws_framedata.h"
namespace dooqu_service
{
namespace basic
{
class ws_client
{
  friend class ws_service;
protected:
  virtual void set_error_code(unsigned short error_code) = 0;
  virtual std::recursive_mutex &get_send_mutex() = 0;
  virtual void set_avaiable(bool is_available) = 0;

public:
  virtual const char *id() = 0;
  virtual const char *name() = 0;
  virtual void write_frame(bool isFin, dooqu_service::basic::ws_framedata::opcode, const char *format, ...) = 0;
  virtual void write_error(unsigned short error_code, char *) = 0;
  virtual void disconnect(unsigned short error_code, char *) = 0;
  virtual int64_t actived_time_elapsed() = 0;
  virtual void *get_command() = 0;
  virtual void set_game_info(void *) = 0;
  virtual void *get_game_info() = 0;
  virtual unsigned short get_error_code() = 0;
  virtual std::recursive_mutex &get_recv_mutex() = 0;
  virtual bool is_availabled() = 0;
  virtual bool can_active() = 0;
  virtual int update_retry_count(bool increase) = 0;
  virtual void set_command_dispatcher(void*) = 0;
  virtual void dispatch_data(char*) = 0;
};
}
}
#endif
