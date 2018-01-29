#include <cstdio>
#include <iostream>
#include <map>
#include <cstring>
#include "dooqu_service.h"

using namespace std;


int main()
{
	
    service_status::create_new();
    service_status::instance()->init(std::this_thread::get_id());
    dooqu_service::service::game_service s(8000);
    s.start();

    getchar();
}
