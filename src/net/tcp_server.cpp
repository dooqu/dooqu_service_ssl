﻿#include "tcp_server.h"

namespace dooqu_service
{
namespace net
{

tcp_server::tcp_server(unsigned int port) :
    acceptor(io_service_),
    context_(boost::asio::ssl::context::sslv23),
    port_(port),
    is_running_(false),
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


void tcp_server::create_worker_thread()
{
    std::thread* worker_thread = new std::thread(std::bind(static_cast<size_t(io_service::*)()>(&io_service::run), &this->io_service_));
    this->threads_status_[worker_thread->get_id()] = new tick_count();
    service_status::instance()->init(worker_thread->get_id());
    worker_threads_.push_back(worker_thread);
    dooqu_service::util::print_success_info("create worker thread {%d}.", worker_thread->get_id());
}


void tcp_server::start_accept()
{
    //提前把game_client变量准备好。该game_client实例如果监听失败，在on_accept的error分支中delete。
	ws_client* client = this->on_create_client();    //投递accept监听
	ws_client_ptr new_client(client, std::bind(&tcp_server::on_destroy_client, this, client));
    this->acceptor.async_accept(client->socket().lowest_layer(), std::bind(&tcp_server::accept_handle, this, std::placeholders::_1, new_client));

    this->is_accepting_ = true;
}


void tcp_server::start()
{
    assert(std::this_thread::get_id() == this->service_thread_id_);

    if (this->is_running_ == false)
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
        this->is_running_ = true;

        int thread_concurrency_count = std::thread::hardware_concurrency() + 1;
        for (int i = 0; i < thread_concurrency_count; i++)
        {
            this->create_worker_thread();
        }
        for (int i = 0; i < thread_concurrency_count; i++)
        {
            this->start_accept();
        }

        this->on_started();
    }
}


void tcp_server::stop_accept()
{
    if (this->is_accepting_ == true)
    {
        boost::system::error_code error;
        this->acceptor.cancel(error);
        if(error)
        {
            std::cout << "cancel error:" << error.message() << std::endl;
        }

        this->acceptor.close(error);
        if(error)
        {
            std::cout << "close error:" << error.message() << std::endl;
        }

        this->is_accepting_ = false;
    }
}


void tcp_server::stop()
{
    assert(std::this_thread::get_id() == this->service_thread_id_);

    if (this->is_running_ == true)
    {
        this->is_running_ = false;
        //不接受新的client
        this->stop_accept();
        //on_stop很重要，它的目的是让信息以加速度缓慢，直至静止的方式，让plugin、zone中的消息停止下来；
        //不要求信号量精，但on_stop之后，相当于在plugin和zone中放置了一个信号量， 各个有惯性的事件，通过读取
        //信号量，逐步的将事件停止下来， so，要在后面1、delete work；2、thread.join; 这样join阻塞完成之时，其实就是
        //所有线程上的事件都已经执行完毕;
        this->on_stop();
        //让所有工作者线程不再空等
        delete this->work_mode_;
        for (int i = 0; i < this->worker_threads_.size(); i++)
        {
            printf("waiting worker thread {%d}", this->worker_threads_.at(i)->get_id());
            this->worker_threads_.at(i)->join();
            printf("\r                                   \r");
            dooqu_service::util::print_success_info("worker thread {%d} returned.", this->worker_threads_.at(i)->get_id());
        }
        //所有线程上的事件都已经执行完毕后， 安全的停止io_service;
        this->io_service_.stop();
        //重置io_service，以备后续可能的tcp_server.start()的再次调用。
        this->io_service_.reset();
        this->on_stoped();
        dooqu_service::util::print_success_info("service stoped successfully.");
    }
}


void tcp_server::on_start()
{
}


void tcp_server::on_started()
{
}


void tcp_server::on_stop()
{
}


void tcp_server::on_stoped()
{
}

void tcp_server::accept_handle(const boost::system::error_code& error, ws_client_ptr client)
{
    if (!error)
    {
        if (this->is_running_)
        {
            std::cout << client->socket().lowest_layer().remote_endpoint().address().to_string() << std::endl;
            //处理新加入的game_client对象，这个时候game_client的available已经是true;
            this->on_client_connected(client.get());
            //处理当前的game_client对象，继续投递一个新的接收请求；
            start_accept();
        }
        else
        {
            this->on_destroy_client(client.get());
        }
    }
    else
    {
        //如果走到这个分支，说明game_server已经调用了stop，停止了服务；
        //但因为start_accept，已经提前投递了N个game_client
        //所以这里返回要对game_client进行销毁处理
        //printf("tcp_server.accept_handle canceled:%s\n", error.message().c_str());
        this->on_destroy_client(client.get());
    }
}


tcp_server::~tcp_server()
{
    for (int i = 0; i < worker_threads_.size(); i++)
    {
        delete threads_status_[worker_threads_.at(i)->get_id()];
        delete worker_threads_.at(i);
    }
}
}
}
