#pragma once

#include <cassandra.h>
#include <ankerl/unordered_dense.h>

struct CassUUIDHash
{
    size_t operator()(const CassUuid &k) const noexcept
    {
        size_t h1 = std::hash<uint64_t>{}(k.clock_seq_and_node);
        size_t h2 = std::hash<uint64_t>{}(k.time_and_version);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

inline bool operator==(const CassUuid &lhs, const CassUuid &rhs) noexcept
{
    return lhs.time_and_version == rhs.time_and_version &&
           lhs.clock_seq_and_node == rhs.clock_seq_and_node;
}

struct IpEntry
{
    std::string countryCode;
    std::vector<uint8_t> ip;

    int priority;

    size_t indexInVector;
};

struct IpGroup
{
    CassUuid version;
};

using EntryMap = ankerl::unordered_dense::map<
    CassUuid,
    ankerl::unordered_dense::map<
        CassUuid,
        IpEntry,
        CassUUIDHash>,
    CassUUIDHash>;

using GroupEntryMap = ankerl::unordered_dense::map<
    CassUuid,
    ankerl::unordered_dense::map<
        std::string,
        std::vector<CassUuid>>,
    CassUUIDHash>;

using GroupMap = ankerl::unordered_dense::map<
    CassUuid,
    IpGroup,
    CassUUIDHash>;

extern EntryMap entries;
extern GroupEntryMap group_entries;
extern GroupMap groups;