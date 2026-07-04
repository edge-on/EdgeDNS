#pragma once

#include <string>

namespace DB
{
    class Record
    {
        static std::string getRecord(std::string name, int type);
    };
} // namespace Cassandra