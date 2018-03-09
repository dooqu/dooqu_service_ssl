#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>
#include <boost/asio.hpp>
#include "../util/tick_count.h"
#include "../util/utility.h"
#include "../basic/ws_service.h"
#include "ws_session.h"


namespace dooqu_service
{
namespace net
{
using namespace boost::asio::ip;
using namespace boost::asio;
using namespace dooqu_service::util;
using namespace dooqu_service::basic;


typedef std::map<std::thread::id, tick_count*> thread_status_map;
typedef std::vector<std::thread*> worker_threads;

template<class SOCK_TYPE>
class tcp_server : public ws_service, boost::noncopyable
{
typedef std::shared_ptr<ws_session<SOCK_TYPE>> ws_session_ptr;
protected:
    enum { MAX_ACCEPTION_NUM = 4 };
    enum {STATE_STOP = 0, STATE_RUNNING = 1, STATE_STOP_PENDING = 2} state_;
    io_service io_service_;
    io_service::work* work_mode_;
    
    unsigned int port_;

    tcp::acceptor acceptor;
    bool is_accepting_;

    //fill all worker thread's tick_count.
    thread_status_map threads_status_;

    //fill all the worker thread.
    worker_threads worker_threads_;

    //service 's createor thread.
    std::thread::id service_thread_id_;

    //ssl context
    boost::asio::ssl::context context_;
    boost::system::error_code err_;

    void create_worker_thread(const boost::system::error_code& err_code)
    {
        std::thread* worker_thread = new std::thread(std::bind(static_cast<size_t(io_service::*)(boost::system::error_code&)>(&io_service::run), &this->io_service_, err_code));
        this->threads_status_[worker_thread->get_id()] = new tick_count();
        service_status::instance()->init(worker_thread->get_id());
        worker_threads_.push_back(worker_thread);
        std::cout << "worker thread 0x" << std::hex << worker_thread->get_id() << " created." << std::endl;
    }


    void start_accept()
    {
        ws_session<SOCK_TYPE>* client = this->on_create_client(); 
        ws_session_ptr new_client(client, std::bind(&tcp_server<SOCK_TYPE>::on_destroy_client, this, client));
        this->acceptor.async_accept(client->socket(), 
        [this, new_client](const boost::system::error_code& error) 
        {
            if (!error)
            {
                if (this->state_ == STATE_RUNNING)
                {
                    std::cout << new_client->socket().lowest_layer().remote_endpoint().address().to_string() << " connected." << std::endl;
                    //处理新加入的game_client对象，这个时候game_client的available已经是true;
                    this->on_client_connected(new_client.get());				
                    //deliver new async accept;
                    start_accept();
                }
            }
            else
            {
                std::cout << "stop accetor" << std::endl;
            }
            //else if error, the new_client will be auto released.
        });
    }

    void stop_accept()
    {
        if (this->acceptor.is_open() == true)
        {
            boost::system::error_code error;
            this->acceptor.cancel(error);
            if (error)
            {
                std::cout << "cancel error:" << error.message() << std::endl;
            }

            this->acceptor.close(error);
            if (error)
            {
                std::cout << "close error:" << error.message() << std::endl;
            }
            //this->is_accepting_ = false;
        }
    }

    virtual ws_session<SOCK_TYPE>* on_create_client() = 0;
    virtual void on_client_connected(ws_session<SOCK_TYPE>* client) = 0;
    virtual void on_destroy_client(ws_session<SOCK_TYPE>*) = 0;
    virtual void on_start(){}
    virtual void on_started(){}
    virtual void on_stop(){}
    virtual void on_stoped(){}

public:
    tcp_server(unsigned int port);

    boost::asio::io_service& get_io_service()
    {
        return this->io_service_;
    };

    thread_status_map* threads_status()
    {
        return &this->threads_status_;
    }
    
    void tick_count_threads()
    {
        int thread_size = this->worker_threads_.size();
        for (int i = 0; i < thread_size; i++)
        {
            this->io_service_.post(std::bind(&tcp_server<SOCK_TYPE>::update_curr_thread, this));
        }
    }

    void update_curr_thread()
    {
        thread_status_map::iterator curr_thread_pair = this->threads_status()->find(std::this_thread::get_id());
        if (curr_thread_pair != this->threads_status()->end())
        {
            curr_thread_pair->second->restart();
            std::cout << "post update at thread 0x" << std::hex << std::this_thread::get_id() << std::endl;
        }
    }

    //server start.
    void start()
    {
        assert(std::this_thread::get_id() == this->service_thread_id_);

        if (this->is_running() == false)
        {
            if(acceptor.is_open() == false)
            {
                tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), this->port_);
                acceptor.open(endpoint.protocol());
                acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));

                boost::system::error_code error;
                acceptor.bind(endpoint, error);

                if(error)
                {
                    std::cout << "server not started, bind error:" << error.message() << std::endl;
                    return;
                }
                acceptor.listen();
            }

            this->on_start();
            this->work_mode_ = new io_service::work(this->io_service_);            

            int thread_concurrency_count = std::thread::hardware_concurrency() + 1;
            for (int i = 0; i < thread_concurrency_count; i++)
            {
                this->create_worker_thread(err_);
            }
            for (int i = 0; i < thread_concurrency_count; i++)
            {
                this->start_accept();
            }
            //this->is_accepting_ = true;
            this->state_ = STATE_RUNNING;            
            this->on_started();
        }
    }


    void stop()
    {
        //assert(std::this_thread::get_id() == this->service_thread_id_);

        if (this->is_running() == true)
        {
            this->state_ == STATE_STOP_PENDING;
            //this->cancel_all_task();
            //不接受新的client
            this->stop_accept();
            //on_stop很重要，它的目的是让信息以加速度缓慢，直至静止的方式，让plugin、zone中的消息停止下来；
            //不要求信号量精，但on_stop之后，相当于在plugin和zone中放置了一个信号量， 各个有惯性的事件，通过读取
            //信号量，逐步的将事件停止下来， so，要在后面1、delete work；2、thread.join; 这样join阻塞完成之时，其实就是
            //所有线程上的事件都已经执行完毕;
            this->on_stop();

            std::cout << "on_stop()" << std::endl;
            //让所有工作者线程不再空等
            delete this->work_mode_;

            std::cout << "delete work_mode" << std::endl;

            for (int i = 0; i < this->worker_threads_.size(); i++)
            {
                std::cout << "waiting for worker thread 0x" << std::hex << this->worker_threads_.at(i)->get_id() << "..." << std::endl; ;
                this->worker_threads_.at(i)->join();
                dooqu_service::util::print_success_info("worker thread {%x} returned.", this->worker_threads_.at(i)->get_id());
            }
            //所有线程上的事件都已经执行完毕后， 安全的停止io_service;
            this->io_service_.stop();
            //重置io_service，以备后续可能的tcp_server.start()的再次调用。
            this->io_service_.reset();
            this->state_ = STATE_STOP;
            this->on_stoped();
            dooqu_service::util::print_success_info("service stoped successfully.");
        }
    }

    inline bool is_running()
    {
        return this->state_ == STATE_RUNNING;
    }

    virtual ~tcp_server()
    {
        for (int i = 0; i < worker_threads_.size(); i++)
        {
            delete threads_status_[worker_threads_.at(i)->get_id()];
            delete worker_threads_.at(i);
        }
    }
};


template<>
inline tcp_server<tcp_stream>::tcp_server(unsigned int port) :
    acceptor(io_service_),
    context_(boost::asio::ssl::context::sslv23),
    port_(port),
    state_(0),
    is_accepting_(false),
    service_thread_id_(std::this_thread::get_id())
{
}


template<>
inline tcp_server<ssl_stream>::tcp_server(unsigned int port) :
    acceptor(io_service_),
    context_(boost::asio::ssl::context::sslv23),
    port_(port),
    state_(0),
    is_accepting_(false),
    service_thread_id_(std::this_thread::get_id())
{
    context_.set_options(
        boost::asio::ssl::context::default_workarounds
        | boost::asio::ssl::context::no_sslv2
        | boost::asio::ssl::context::single_dh_use);
    //context_.set_password_callback(boost::bind(&server::get_password, this));
    context_.use_certificate_chain_file("server.pem");
    context_.use_private_key_file("server.key", boost::asio::ssl::context::pem);
}


template<>
inline void tcp_server<ssl_stream>::start_accept()
{
    ws_session<ssl_stream> *client = this->on_create_client();
    ws_session_ptr new_client(client, std::bind(&tcp_server<ssl_stream>::on_destroy_client, this, client));
    this->acceptor.async_accept(client->socket().lowest_layer(),
                                [this, new_client](const boost::system::error_code &error) {
                                    if (!error)
                                    {
                                        if (this->state_ == STATE_RUNNING)
                                        {
                                            std::cout << new_client->socket().lowest_layer().remote_endpoint().address().to_string() << " connected." << std::endl;
                                            //处理新加入的game_client对象，这个时候game_client的available已经是true;
                                            this->on_client_connected(new_client.get());
                                            //deliver new async accept;
                                            start_accept();
                                        }
                                    }
                                    else
                                    {
                                        std::cout << "stop accetor" << std::endl;
                                    }
                                    //else if error, the new_client will be auto released.
                                });
}

}
}
#endif
