#ifndef __ASYNC_TASK_H__
#define __ASYNC_TASK_H__

#include <set>
#include <deque>
#include <mutex>
#include "boost/asio.hpp"
#include "../util/tick_count.h"
#include "../util/utility.h"


namespace dooqu_service
{
namespace service
{
using namespace dooqu_service::util;
using namespace boost::asio;
class async_task : boost::noncopyable
{
public:
    class task_timer : public boost::asio::deadline_timer
    {
    protected:
        bool cancel_enabled_;
        bool disposed_;

    public:
        tick_count last_actived_time;
        static task_timer* create(io_service& ios, bool cancel_enabled = false)
        {
            void* timer_chunk = boost::singleton_pool<task_timer, sizeof(task_timer)>::malloc();//
            if(timer_chunk != NULL)
            {
                task_timer* timer = new(timer_chunk) task_timer(ios, cancel_enabled);
                return timer;
            }
            return NULL;
        }

        static void destroy(task_timer* timer)
        {
            if(timer != NULL)
            {
                timer->~task_timer();
                boost::singleton_pool<task_timer, sizeof(task_timer)>::free(timer);
            }
        }

        task_timer(io_service& ios, bool cancel_enabled = false) : deadline_timer(ios)
        {
            this->cancel_enabled_ = cancel_enabled;
            this->disposed_ = false;
        }

        virtual ~task_timer()
        {
            //printf("~task_timer\n");
        }

        bool is_cancel_eanbled()
        {
            return this->cancel_enabled_;
        }
    };

public:
    async_task(io_service& ios);
    virtual ~async_task();

protected:
    io_service& task_io_service_;
    //timer池中最小保有的数量，timer_.size > 此数量后，会启动空闲timer检查
    const static int MIN_ACTIVED_TIMER = 100;
    //如timer池中的数量超过{MIN_ACTIVED_TIMER}定义的数量， 并且队列后部的timer空闲时间超过
    //MAX_TIMER_FREE_TICK的值，会被强制回收
    const static int MAX_TIMER_FREE_TICK = 1 * 60 * 1000;
    //存放定时器的双向队列
    //越是靠近队列前部的timer越活跃，越是靠近尾部的timer越空闲
    std::deque<task_timer*> free_timers_;

    std::set<task_timer*> working_timers_;
    //timer队列池的状态锁
    std::recursive_mutex free_timers_mutex_;

    std::recursive_mutex working_timers_mutex_;

    task_timer* queue_task_internal(std::function<void(void)> callback_handle, int millisecs_wait, bool cancel_enabled, bool sys_call);


    ///queue a new task_timer, and it will internal call queue_task_internal with false for sys_call argument.
    ///
    ///
    task_timer* queue_task(std::function<void(void)> callback_handle, int millisecs_wait, bool cancel_enabled = false);

    ///cancel special timer. the timer argument must be enabled_canceled, otherwise return false
    ///
    ///
    bool cancel_task(task_timer* timer);

    /// cancel all task in running ppol.
    /// will not effect the new task_timer after invoke.
    ///
    void cancel_all_task();


    ///queue task's internal callback, and it will invoke user callback.
    ///if the error is false, and  sys_call is true、or can_callback() is true,user callback will be invoke
    ///
    void task_handle(const boost::system::error_code& error, task_timer* timer_, std::function<void(void)> callback_handle, bool sys_call);

    /// can_callback is a virtual method, implement by child class,
    /// and indicat the user callback can be invoke,wether or not. 
    /// in the children service class, it normaly return the service status.
    ///
    virtual bool can_async_task_callback()
    {
        return true;
    }
};
}
}


#endif // TASK_TIMER_H
