#pragma once

#include <sys/socket.h>
#include <sys/epoll.h>

#include <unistd.h>

#include <arpa/inet.h>

#include <iostream>
#include <iomanip>
#include <vector>

#include "Core/EoD.hpp"

#include "DNS/DNS.hpp"
#include "DNS/RRL.hpp"
#include "DNS/Zone.hpp"

#include "Cassandra/Cassandra.hpp"

#include "Utils/Vector.hpp"
#include "Utils/String.hpp"

#include "Core/RData/RData.hpp"

#include "Global/Static.hpp"

#include "Zone/Domain.hpp"

#include "DNS/Geo/Group.hpp"

class Main
{
public:
    static Cassandra *cas;
    static inline uint32_t next_zone_id = 1;
};