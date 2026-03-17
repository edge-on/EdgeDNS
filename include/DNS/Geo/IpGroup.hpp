#pragma once

#include <cassandra.h>
#include <ankerl/unordered_dense.h>

struct IpEntry
{
    std::string countryCode;
    std::vector<uint8_t> ip;

    int priority;
};

struct IpGroup
{
    CassUuid version;
};

struct UUIDHash
{
    size_t operator()(const CassUuid &k) const noexcept
    {
        return std::hash<uint64_t>()(k.clock_seq_and_node) ^
               std::hash<uint64_t>()(k.time_and_version);
    }
};

using EntryMap = ankerl::unordered_dense::map<
    CassUuid,
    IpEntry,
    UUIDHash
>;

using GroupEntryMap = ankerl::unordered_dense::map<
    CassUuid,
    ankerl::unordered_dense::map<
        std::string,
        std::vector<CassUuid>
        >,
    UUIDHash>;

using GroupMap = ankerl::unordered_dense::map<
    CassUuid,
    IpGroup,
    UUIDHash>;

extern EntryMap entries;
extern GroupEntryMap group_entries;
extern GroupMap groups;