#pragma once

#include <string>
#include <vector>

#include "index.hpp"

#include "Cache/MmapRecords.hpp"

namespace DB
{
    class Record
    {
    public:
        static std::vector<Records::DNSResponseData> getRecord(std::string zone, std::string name, int type);
    };
} // namespace Cassandra