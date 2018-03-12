#include "game_session.h"
namespace dooqu_service
{
namespace service
{
template<class SOCK_TYPE>
std::thread::id game_session<SOCK_TYPE>::thread_id_none = std::thread::id();
}
}