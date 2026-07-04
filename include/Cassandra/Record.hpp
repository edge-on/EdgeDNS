#pragma once

#include <string>
#include <vector>

#include "index.hpp"

#include "Cache/Mmap.hpp"

namespace DB
{
    class Record
    {
    public:
        static std::vector<DNSResponseData> getRecord(std::string zone, std::string name, int type);
    };
} // namespace Cassandra