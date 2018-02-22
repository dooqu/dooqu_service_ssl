#ifndef __GAME_SERVICE_H__
#define __GAME_SERVICE_H__

#include <set>
#include <queue>
#include <list>
#include <map>
#include <thread>
#include <mutex>
#include <functional>
#include <boost/asio/deadline_timer.hpp>
#include <boost/pool/singleton_pool.hpp>
#include "../net/tcp_server.h"
#include "../service/command_dispatcher.h"
#include "../util/utility.h"
#include "../util/char_key_op.h"
#include "service_error.h"
#include "async_task.h"
#include "http_request.h"
#include "game_client.h"
#include "game_plugin.h"

using namespace boost::asio;
using namespace dooqu_service::net;
using namespace std::placeholders;

namespace dooqu_service
{
namespace service
{
typedef std::function<void(const boost::system::error_code&, const int, const string&)> req_callback;
struct http_request_task
{
    string request_host;
    string request_path;
    req_callback callback;
    http_request_task(const char* host, const char* path, req_callback user_callback) :
        request_host(host), request_path(path), callback(user_callback)
    {
    }

    void reset(const char* host, const char* path,req_callback user_callback)
    {
        request_host.assign(host);
        request_path.assign(path);
        callback = user_callback;
    }
};

class game_zone;
class game_plugin;


template<class SOCK_TYPE>
class game_service : public command_dispatcher, public async_task, public tcp_server<SOCK_TYPE>
{
public:
    typedef std::shared_ptr<game_session<SOCK_TYPE> > game_client_ptr;
    typedef std::map<const char*, game_plugin*, char_key_op> game_plugin_map;
    typedef std::list<game_plugin*> game_plugin_list;
    typedef std::map<const char*, game_zone*, char_key_op> game_zone_map;
    typedef std::set<game_client_ptr> game_client_map;

protected:
    enum { MAX_AUTH_SESSION_COUNT = 50 };
    game_plugin_map plugins_;
    game_plugin_list plugin_list_;
    game_zone_map zones_;
    game_client_map clients_;
    std::mutex clients_mutex_;
    std::mutex plugins_mutex_;
    std::mutex zones_mutex_;
    std::mutex destroy_list_mutex_;

    boost::asio::deadline_timer check_timeout_timer;

    //std::set<http_request*> http_request_working_;
    void* request_block_[2];
    std::queue<void*> request_pool_[2];
    std::recursive_mutex request_pool_mutex_[2];

    std::recursive_mutex mutex_temp_;
    std::queue<http_request_task*> task_queue_;
    std::recursive_mutex task_queue_mutex_;
//    std::recursive_mutex update_tasks_mutex_;

    /*start_events*/
    virtual void on_start();
    virtual void on_started();
    virtual void on_stop();
    virtual void on_stoped();
    inline virtual void on_client_command(game_session<SOCK_TYPE>* client, command* command);
    virtual ws_session<SOCK_TYPE>* on_create_client();
    virtual void on_client_connected(ws_session<SOCK_TYPE>* client);
	//virtual int on_client_handshake(ws_client* client, ws_request* req);
    virtual void on_client_leave(game_session<SOCK_TYPE>* client, int code);
    virtual void on_destroy_client(ws_session<SOCK_TYPE>*);
    virtual void on_check_timeout_clients(const boost::system::error_code &error);
    /*end_events;*/

    /*start functions*/
    void* malloc_http_request(int pool_index);
    void free_http_request(int pool_index, void* request);
    void http_request_handle(const boost::system::error_code& err, const int status_code,  void* req, req_callback callback);
    void queue_http_request_handle(const boost::system::error_code& err, const int status_code, http_request* req, req_callback callback);
    /*end_functions*/
    //非虚方法
    void begin_auth(game_plugin* plugin, game_session<SOCK_TYPE>* client, command* cmd);
    void end_auth(const boost::system::error_code& code, const int status_code, const std::string& response_string, const char* plugin_id, game_session<SOCK_TYPE>* client);
    void dispatch_bye(ws_client* client);
    void post_handle(std::function<void(void)> handle);

    //应用层注册
    void client_login_handle(game_session<SOCK_TYPE>* client, command* command);
    void robot_login_handle(game_session<SOCK_TYPE>* client, command* command);

public:
    game_service(unsigned int port);
    virtual ~game_service();
    int load_plugin(game_plugin* game_plugin, char* zone_id, char** errorMsg = NULL);
    bool unload_plugin(game_plugin* game_plugin, int seconds_wait_for_complete = -1);
    virtual bool start_http_request(const char* host, const char* path, req_callback callback);
    virtual bool queue_http_request(const char* host, const char* path, req_callback callback);
    void post_handle(std:: function<void(void)> handle, int millisecs_delay, bool cancel_enable = false)
    {
        this->async_task::queue_task(handle, millisecs_delay, cancel_enable);
    }
    inline game_plugin_list* get_plugins()
    {
        return &this->plugin_list_;
    }

};

template<>
inline game_service<tcp_stream>::game_service(unsigned int port) :
    tcp_server<tcp_stream>(port), async_task(this->get_io_service()), check_timeout_timer(get_io_service())
{
    std::cout << "game_service" << '\a' << std::endl;
}

template<>
inline game_service<ssl_stream>::game_service(unsigned int port) :
    tcp_server<ssl_stream>(port), async_task(this->get_io_service()), check_timeout_timer(get_io_service())
{
        std::cout << "game_service" << '\a' << std::endl;
}

template<class SOCK_TYPE>
int game_service<SOCK_TYPE>::load_plugin(game_plugin* game_plugin, char* zone_id, char** errorMsg)
{
    assert(std::this_thread::get_id() == this->service_thread_id_);

    if (game_plugin == NULL || game_plugin->game_id() == NULL || game_plugin->game_service_ != this)
    {
        //dooqu_service::util::print_error_info("plugin not created, argument error.");
        (*errorMsg) = "plugin's argument error.";
        return -1;
    }

    if(game_plugin->is_onlined_ || game_plugin->clients()->size() > 0)
    {
        //dooqu_service::util::print_error_info("plugin {%s} is in using.", game_plugin->game_id());
        (*errorMsg) = "plugin is in use.";
        return -2;
    }

    ___lock___(this->plugins_mutex_, "create_plugin");

    game_plugin_map::iterator curr_plugin = this->plugins_.find(game_plugin->game_id());

    if (curr_plugin != this->plugins_.end())
    {
        //dooqu_service::util::print_error_info("plugin's id {%s} is in use.", game_plugin->game_id());
        (*errorMsg) = "plugin is in use.";
        return -2;
    }

    this->plugins_[game_plugin->game_id()] = game_plugin;
    this->plugin_list_.push_back(game_plugin);

    //如果传递的zoneid不为空，那么为游戏挂载游戏区
    if (zone_id != NULL)
    {
        game_zone_map::iterator curr_zone = this->zones_.find(zone_id);

        if (curr_zone != this->zones_.end())
        {
            game_plugin->zone_ = (*curr_zone).second;
        }
        else
        {
            game_zone* zone = game_plugin->on_create_zone(zone_id);

            if (this->is_running_ && zone != NULL)
            {
                zone->load();
            }
            this->zones_[zone->get_id()] = zone;
            game_plugin->zone_ = zone;
        }
    }

    if (this->is_running_)
    {
        game_plugin->load();
    }

    return 1;
}


/*
unload the plugin, remove the plugin from service's plugin list.
and call plugin's onunload event if plugin not unload.
*/
template<class SOCK_TYPE>
bool game_service<SOCK_TYPE>::unload_plugin(game_plugin* plugin, int seconds_for_wait_compl)
{
    assert(std::this_thread::get_id() == this->service_thread_id_);

    if(plugin == NULL || plugin->game_id() == NULL)
        return false;

    {
        ___lock___(this->plugins_mutex_, "game_service::unload_plugin::plugin_mutex_");
        if(this->plugins_.erase(plugin->game_id()) <= 0)
            return false;

        this->plugin_list_.remove(plugin);
    }

    if(plugin->is_onlined())
    {
        plugin->unload();
    }

    int millisecs_for_wait_compl = seconds_for_wait_compl * 1000;
    int millisecs_sleeped = 0;

    for(;;)
    {
        if(plugin->clients()->size() <= 0)
            return true;

        if(seconds_for_wait_compl <= -1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        else if(seconds_for_wait_compl == 0)
        {
            return false;
        }
        else
        {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if((millisecs_sleeped += 100) <  millisecs_for_wait_compl)
            {
                continue;
            }
            return false;
        }
    }
}

template<class SOCK_TYPE>
void game_service<SOCK_TYPE>::on_start()
{
    std::srand((unsigned)time(NULL));

    this->regist_handle("LOG", make_handler(game_service::client_login_handle));
    this->regist_handle("RLG", make_handler(game_service::robot_login_handle));
    //加载所有的所有分区
    for (game_zone_map::iterator curr_zone = this->zones_.begin();
            curr_zone != this->zones_.end(); ++curr_zone)
    {
        (*curr_zone).second->load();
    }

    {
        ___lock___(this->plugins_mutex_, "game_servcie::on_init::plugin_mutex");
        //加载所有的游戏逻辑
        for (game_plugin_list::iterator curr_game = this->plugin_list_.begin();
                curr_game != this->plugin_list_.end(); ++curr_game)
        {
            (*curr_game)->load();
        }
    }

	unsigned int obj_size = sizeof(http_request);
    // void* block = malloc(obj_size * MAX_AUTH_SESSION_COUNT * 2);
    for(int i = 0; i < 2; i++)
    {
        request_block_[i] = malloc(obj_size * MAX_AUTH_SESSION_COUNT);
        for(int j = 0; j < MAX_AUTH_SESSION_COUNT; j++)
        {
            this->request_pool_[i].push((void*)(request_block_[i] + (j * obj_size)));
        }
    }
    //std::cout << "service_status:" << & service_status_ << std::endl;
}

template<class SOCK_TYPE>
void game_service<SOCK_TYPE>::on_started()
{
    this->check_timeout_timer.expires_from_now(boost::posix_time::seconds(5));
    this->check_timeout_timer.async_wait(std::bind(&game_service<SOCK_TYPE>::on_check_timeout_clients, this, std::placeholders::_1));
}

template<class SOCK_TYPE>
void game_service<SOCK_TYPE>::on_stop()
{
    //停止对登录超时的检测
    this->check_timeout_timer.cancel();
    //清理临时登录用户的内存资源
    {
        ___lock___(this->clients_mutex_, "game_service::on_stop::client_mutex");
        for (typename game_client_map::iterator curr_client = this->clients_.begin();
                curr_client != this->clients_.end(); ++curr_client)
        {
            game_client_ptr client = (*curr_client);
            client->disconnect((uint16_t)service_error::WS_ERROR_GOING_AWAY, NULL);
        }
    }

    {
        ___lock___(this->plugins_mutex_, "game_service::on_stop::plugins_mutex");
        for (game_plugin_list::iterator curr_game = this->plugin_list_.begin();
                curr_game != this->plugin_list_.end(); ++curr_game)
        {
            (*curr_game)->unload();
        }
    }
    ////boost::this_thread::sleep(boost::posix_time::milliseconds(500));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    //停止所有的游戏区
    for (game_zone_map::iterator curr_zone = this->zones_.begin();
            curr_zone != this->zones_.end(); ++curr_zone)
    {
        (*curr_zone).second->unload();
    }
}
template<class SOCK_TYPE>
void game_service<SOCK_TYPE>::on_stoped()
{
    //on_destroy_clients_in_destroy_list(true);
}

template<class SOCK_TYPE>
void game_service<SOCK_TYPE>::dispatch_bye(ws_client* client)
{
    std::cout << "dispatch bye" << std::endl;
    game_session<SOCK_TYPE>* game_client = (game_session<SOCK_TYPE>*)client;
    this->on_client_leave(client, game_client->get_error_code());
}

template<class SOCK_TYPE>
void game_service<SOCK_TYPE>::on_client_command(game_session<SOCK_TYPE>* client, command* command)
{
    if (this->is_running_ == false)
        return;

    if (client->can_active() == false)
    {
        client->disconnect((uint16_t)service_error::CONSTANT_REQUEST, NULL);
        return;
    }
    command_dispatcher::on_client_command(client, command);
}


template<>
inline ws_session<tcp_stream>* game_service<tcp_stream>::on_create_client()
{
    //std::cout << "创建game_client at MAIN." << std::endl;
    void* client_mem = memory_pool_malloc<game_session<tcp_stream>>();//boost::singleton_pool<game_client, sizeof(game_client)>::malloc();
    assert(client_mem != NULL);
    return new(client_mem) game_session<tcp_stream>(this->get_io_service());
}

template<>
inline ws_session<ssl_stream>* game_service<ssl_stream>::on_create_client()
{ 
    //std::cout << "创建game_client at MAIN." << std::endl;
    void* client_mem = memory_pool_malloc<game_session<ssl_stream>>();//boost::singleton_pool<game_client, sizeof(game_client)>::malloc();
    assert(client_mem != NULL);
    return new(client_mem) game_session<ssl_stream>(this->get_io_service(), context_);
}


template<class SOCK_TYPE>
inline void game_service<SOCK_TYPE>::on_client_connected(ws_session<SOCK_TYPE>* t_client)
{
    game_session<SOCK_TYPE>* client = (game_session<SOCK_TYPE>*)t_client;
    client->set_available(true);
    //设置命令的监听者为当前service
    client->set_command_dispatcher(this);
    {
		___lock___(this->clients_mutex_, "game_service::on_client_join");
        //在登录用户组中注册
        this->clients_.insert(client->shared_from_self());
    }//对用户组上锁

    client->active();
    client->start();
}


template<class SOCK_TYPE>
inline void game_service<SOCK_TYPE>::on_client_leave(game_session<SOCK_TYPE>* client, int leave_code)
{
    printf("{%s} leave game_service. code={%d}\n", client->id(), leave_code);
    client->set_command_dispatcher(NULL);
    {
        ___lock___(this->clients_mutex_, "game_service::on_client_leave::clients_mutex");
        this->clients_.erase(client->shared_from_self());
    }
}

template<class SOCK_TYPE>
inline void game_service<SOCK_TYPE>::on_destroy_client(ws_session<SOCK_TYPE>* t_client)
{
    game_session<SOCK_TYPE>* client = (game_session<SOCK_TYPE>*)t_client;
	if (client->read_pos_ != -1)
	{
		std::cout << "client destroy error." << std::endl;
	}
    client->~game_session<SOCK_TYPE>();
    memory_pool_free<game_session<SOCK_TYPE>>(client);
}


//使用了定时器
template<class SOCK_TYPE>
inline void game_service<SOCK_TYPE>::on_check_timeout_clients(const boost::system::error_code &error)
{
    return;
    if (!error && this->is_running_)
    {
        //如果禁用超时检测，请注释return;
        if(this->clients_.size() > 0)
        {
            ___lock___(this->clients_mutex_, "game_service::on_check_timeout_clients::clients_mutex");
            //对所有用户的active时间进行对比，超过20秒还没有登录动作的用户被装进临时数组
            for (typename game_client_map::iterator curr_client = this->clients_.begin();
                    curr_client != this->clients_.end(); ++curr_client)
            {
                game_client_ptr client = (*curr_client);
                if (client->command_dispatcher_ == this && client->actived_time_elapsed() > 5 * 1000)
                {
                    uint16_t ret = service_error::TIME_OUT;
                    char* reason = NULL;
                    this->post_handle(std::bind(&game_session<SOCK_TYPE>::disconnect, client, ret, reason));
                }
            }
        }

        static tick_count timeout_count;

        if (timeout_count.elapsed() > 60 * 1000)
        {
            ___lock___(this->plugins_mutex_, "game_service::on_check_timeout_clients::plugins_mutex");
            for (game_plugin_list::iterator curr_game = this->get_plugins()->begin();
                    curr_game != this->get_plugins()->end();
                    ++curr_game)
            {
                (*curr_game)->on_update_timeout_clients();
            }
            timeout_count.restart();
        }

        //如果服务没有停止， 那么继续下一次计时
        if (this->is_running_)
        {
            this->check_timeout_timer.expires_from_now(boost::posix_time::seconds(15));
            this->check_timeout_timer.async_wait(std::bind(&game_service::on_check_timeout_clients, this, std::placeholders::_1));
        }
    }
}

template<class SOCK_TYPE>
inline bool game_service<SOCK_TYPE>::queue_http_request(const char* host, const char* path, req_callback callback)
{
    std::cout << "queue http request" << std::endl;
    void* req_block = this->malloc_http_request(1);
    if(req_block != NULL)
    {
        http_request* request = new(req_block) http_request(this->get_io_service(),
                host,
                path,
                std::bind(&game_service<SOCK_TYPE>::queue_http_request_handle, this, std::placeholders::_1, std::placeholders::_2, (http_request*)req_block, callback));
        return true;
    }
    else
    {         
        void* task_chunk = memory_pool_malloc<http_request_task>();
        assert(task_chunk != NULL);
        http_request_task* req_task = new(task_chunk) http_request_task(host, path, callback);
        ___lock___(this->task_queue_mutex_, "game_servcie::queue_http_request::task_queue_mutex_");
        this->task_queue_.push(req_task);
		return false;
    }
}

template<class SOCK_TYPE>
inline void game_service<SOCK_TYPE>::queue_http_request_handle(const boost::system::error_code& err, const int status_code, http_request* req, req_callback callback)
{
    //std::cout << err << std::endl;
    //std::cout << "SO queue handle" << std::endl;
    string response_content;
    if(!err && status_code == 200)
    {
        req->read_response_content(response_content);
    }    
    callback(err, status_code, response_content);
    req->~http_request();
    this->free_http_request(1, req);

    ___lock___(this->task_queue_mutex_, "game_servcie::queue_http_request_handle::task_queue_mutex_");
    while(this->task_queue_.empty() == false)
    {
        void* req_block = this->malloc_http_request(1);
        if(req_block != NULL)
        {
            http_request_task* req_task = this->task_queue_.front();

            http_request* request = new(req_block) http_request(this->get_io_service(),
                    req_task->request_host.c_str(),
                    req_task->request_path.c_str(),
                    std::bind(&game_service::queue_http_request_handle, this, std::placeholders::_1, std::placeholders::_2, (http_request*)req_block, req_task->callback));

            this->task_queue_.pop();
            req_task->~http_request_task();
            memory_pool_free<http_request_task>(req_task);
            continue;
        }
        break;
    }
}

template<class SOCK_TYPE>
inline bool game_service<SOCK_TYPE>::start_http_request(const char* host, const char* path, req_callback callback)
{
    void* req_block = this->malloc_http_request(0); 
    if(req_block == NULL)
    {
        return false; 
    }
          
    http_request* request = NULL;
    request = new(req_block) http_request(this->get_io_service(),
            host,
            path,
            std::bind(&game_service<SOCK_TYPE>::http_request_handle, this, std::placeholders::_1, std::placeholders::_2, req_block, callback));    
    return true;
}

template<class SOCK_TYPE>
inline void game_service<SOCK_TYPE>::http_request_handle(const boost::system::error_code& err, const int status_code, void* req_block, req_callback callback)
{
    string response_content;
    if(!err && status_code == 200)
    {
        http_request* req = (http_request*)req_block;
        req->read_response_content(response_content);
    }    
    callback(err, status_code, response_content);
    this->free_http_request(0, req_block);
}

template<class SOCK_TYPE>
inline void* game_service<SOCK_TYPE>::malloc_http_request(int pool_index)
{
    ___lock___(this->request_pool_mutex_[pool_index], "game_servcie::malloc_http_request::request_pool_mutex_");
    if(this->request_pool_[pool_index].empty())
        return NULL;

    void* req = this->request_pool_[pool_index].front();
    this->request_pool_[pool_index].pop();  
    return req;
}

template<class SOCK_TYPE>
inline void game_service<SOCK_TYPE>::free_http_request(int pool_index, void* request)
{
    ___lock___(this->request_pool_mutex_[pool_index], "game_servcie::free_http_request");
    this->request_pool_[pool_index].push(request);
}

template<class SOCK_TYPE>
inline void game_service<SOCK_TYPE>::begin_auth(game_plugin* plugin, game_session<SOCK_TYPE>* client, command* cmd)
{
    const char* game_id = plugin->game_id();
	std::shared_ptr<game_session<SOCK_TYPE>> self = client->shared_from_self();
    bool ret = this->start_http_request("service.wechat.dooqu.com", "/", 
    [self, this, game_id](const boost::system::error_code& error, const int status_code, const std::string& response_string)
    {
        if(!error && status_code == 200)
        {
            game_session<SOCK_TYPE>* client = self.get();
            ___lock___(client->recv_lock_, "end_auth::commander_mutex");
            {
                if(client->available() == false)
                    return;
                //上锁检查，因为plugin有可能已经被卸载
                ___lock___(this->plugins_mutex_, "game_service::end_auth::plugin_mutex");
                {
                    unsigned short ret = service_error::NO_ERROR;
                    //检查当前要登录的plugin是否还有效
                    game_plugin_map::iterator finder = this->plugins_.find(game_id);
                    if(finder != this->plugins_.end())
                    {
                        ret = (*finder).second->auth_client(self.get(), response_string);
                    }
                    else
                    {
                        ret = service_error::GAME_NOT_EXISTED;
                    }

                    if(ret != service_error::NO_ERROR)
                    {
                        self->disconnect(ret, NULL);
                    }
                }
            }
        }
        else
        {
            self->disconnect((unsigned short)service_error::LOGIN_SERVICE_ERROR, NULL);
        }
    });

    if(ret == false)
    {
        uint16_t code = service_error::LOGIN_CONCURENCY_LIMIT;
        client->disconnect(code, NULL);
    }
}

template<class SOCK_TYPE>
inline void game_service<SOCK_TYPE>::end_auth(const boost::system::error_code& code, const int status_code, const std::string& response_string, const char* plugin_id, game_session<SOCK_TYPE>* client)
{
    if (this->is_running_ == false)
        return;

    uint16_t ret = service_error::NO_ERROR;

    {
        //如果clients_中没有找到client，那么返回，说明client已经销毁了
        ___lock___(this->clients_mutex_, "game_service::end_auth::clients_mutex");
        if(this->clients_.find(client->shared_from_self()) != this->clients_.end())
            return;
    }

    if (!code && status_code == 200)
    {
        //检查client是不是已经在过程中断开
        ___lock___(client->recv_lock_, "end_auth::commander_mutex");
        {
            if(client->available() == false)
                return;
            //上锁检查，因为plugin有可能已经被卸载
            ___lock___(this->plugins_mutex_, "game_service::end_auth::plugin_mutex");
            {
                //检查当前要登录的plugin是否还有效
                game_plugin_map::iterator finder = this->plugins_.find(plugin_id);
                if(finder != this->plugins_.end())
                {
                    ret = (*finder).second->auth_client(client, response_string);
                }
                else
                {
                    ret = service_error::GAME_NOT_EXISTED;
                }
            }
        }
    }
    else
    {
        ret = service_error::LOGIN_SERVICE_ERROR;
    }

    if (ret != service_error::NO_ERROR)
    {
        client->disconnect(ret, NULL);
        printf("authentication failed, error code is: %d\n", ret);
    }
}

template<class SOCK_TYPE>
void game_service<SOCK_TYPE>::post_handle(std::function<void(void)> handle)
{
    this->io_service_.post(handle);
}


template<class SOCK_TYPE>
inline void game_service<SOCK_TYPE>::client_login_handle(game_session<SOCK_TYPE>* client, command* command)
{
    if (command->param_size() != 2)
    {
        uint16_t ret = service_error::COMMAND_ERROR;
        client->disconnect(ret, NULL);
        return;
    }

    /*检查该玩家是否已经发送过LOG命令， 防止在begin_auth异步去做其他事情的时候，该client又发起LOG操作*/
    /*至于位什么不用上锁， 是因为，本质上，单个用户的命令是串行的，不会产生并发冲突*/
    if(client->plugin_addr_ != 0)
    {
        client->disconnect((unsigned short)service_error::CLIENT_HAS_LOGINED, NULL);
        return;
    }

    ___lock___(this->plugins_mutex_, "game_service::client_login_handle::plugins_mutex");
    //查找是否存在对应玩家想加入的的game_plugin
    game_plugin_map::iterator it = this->plugins_.find(command->params(0));
    if (it != this->plugins_.end())
    {
        client->fill(command->params(1), command->params(1), NULL);
        game_plugin* plugin = (*it).second;
        //设定plugin_addr_地址，防止用户重复发送LOG命令
        client->plugin_addr_ = reinterpret_cast<int>(plugin);
        //把目标plugin的id带上，在回调函数中要再次检查plugin是否还在
        this->begin_auth(plugin, client, command);
        //在认证期间用户发送的命令将被忽略
        //client->set_command_dispatcher(NULL);
    }
    else
    {
        client->disconnect((uint16_t)service_error::GAME_NOT_EXISTED, NULL);
    }
}

template<class SOCK_TYPE>
inline void game_service<SOCK_TYPE>::robot_login_handle(game_session<SOCK_TYPE>* client, command* command)
{
}

template<class SOCK_TYPE>
inline game_service<SOCK_TYPE>::~game_service()
{
    if (this->is_running_ == true)
    {
        this->stop();
    }

    this->plugins_.clear();
    //销毁所有的游戏区
    for (game_zone_map::iterator curr_zone = this->zones_.begin();
            curr_zone != this->zones_.end(); ++curr_zone)
    {
        delete (*curr_zone).second;
    }
    this->zones_.clear();
    for (thread_status_map::iterator pos_thread_status_pair = this->threads_status_.begin();
            pos_thread_status_pair != this->threads_status_.end();
            ++pos_thread_status_pair)
    {
        delete (*pos_thread_status_pair).second;
    }
    this->threads_status_.clear();

    free(this->request_block_[0]);
    free(this->request_block_[1]);
}

}
}
#endif
