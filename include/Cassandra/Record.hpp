#pragma once

#include <string>
#include <vector>

#include "Cache/Mmap.hpp"

namespace DB
{
    class Record
    {
        static std::vector<DNSResponseData> getRecord(std::string zone, std::string name, int type);
    };
} // namespace Cassandra