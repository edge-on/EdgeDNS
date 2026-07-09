#pragma once

#include <sys/socket.h>
#include <sys/epoll.h>

#include <unistd.h>

#include <arpa/inet.h>

#include <iostream>
#include <iomanip>
#include <vector>

#include "Cache/MmapRecords.hpp"
#include "Cache/MmapSystem.hpp"

#include "Core/Core.hpp"

#include "DNS/RRL.hpp"

#include "Cassandra/Cassandra.hpp"

#include "Utils/Vector.hpp"
#include "Utils/String.hpp"

#include "Core/RData/RData.hpp"

#include "Global/Static.hpp"

#include "Zone/Domain.hpp"

#include "DNS/Geo/Group.hpp"

#include "DNS/Proxy/Proxy.hpp"

class Main
{
public:
    static Cassandra *cas;
    static Mmap *recordsMap;
    static System::Mmap *systemMap;
};