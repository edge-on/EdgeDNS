#include <sys/socket.h>
#include <sys/epoll.h>

#include <unistd.h>

#include <arpa/inet.h>

#include <iostream>
#include <iomanip>
#include <vector>

#include "Core/EoD.hpp"
#include "DNS/DNS.hpp"
#include "Cassandra/Cassandra.hpp"
#include "Utils/Vector.hpp"

class Main
{
public:
    Main();
    ~Main();
};

static uint32_t next_zone_id = 1;