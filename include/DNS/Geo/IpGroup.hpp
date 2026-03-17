#pragma once

#include <cassandra.h>
#include <ankerl/unordered_dense.h>

struct IpEntry
{
    std::string countryCode;
    std::vector<uint8_t> ip;
    int priority;
};

struct UUIDHash
{
    size_t operator()(const CassUuid &k) const noexcept
    {
        return std::hash<uint64_t>()(k.clock_seq_and_node) ^
               std::hash<uint64_t>()(k.time_and_version);
    }
};

using GroupMap = ankerl::unordered_dense::map<
    CassUuid, // group_id
    ankerl::unordered_dense::map<
        std::string,         // location code (DE, USA, ...)
        std::vector<IpEntry> // IP entries
        >,
    UUIDHash>;

extern GroupMap groups;