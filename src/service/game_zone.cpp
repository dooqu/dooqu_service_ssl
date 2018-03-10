#include "game_zone.h"
namespace dooqu_service
{
namespace service
{
bool game_zone::LOG_TIMERS_INFO = false;

game_zone::game_zone(ws_service* service, const char* id)
{
    int n = std::strlen(id);
    this->id_ = new char[n + 1];
    std::strcpy(this->id_, id);
    this->id_[n] = '\0';

    this->game_service_ = service;
    this->is_onlined_ = false;
}


void game_zone::load()
{
    if (this->is_onlined_ == false)
    {
        this->is_onlined_ = true;
        this->on_load();
        print_success_info("zone {%s} loaded.", this->get_id());
    }
}


void game_zone::unload()
{
    if (this->is_onlined_ == true)
    {
        this->is_onlined_ = false;
        this->on_unload();
        print_success_info("zone {%s} unloaded.", this->get_id());
    }
}


void game_zone::on_load()
{
}


void game_zone::on_unload()
{
}

game_zone::~game_zone()
{
    delete[] this->id_;

    std::cout << "~game_zone" << std::endl;
}
}
}
