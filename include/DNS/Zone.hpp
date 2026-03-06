#pragma once

#include <cassandra.h>

#include <ankerl/unordered_dense.h>

struct Record
{
    uint16_t type;
    uint32_t ttl;
    std::vector<uint8_t> rdata;
};

struct ByteVecHash
{
    uint64_t operator()(std::vector<uint8_t> const &v) const noexcept
    {
        if (v.empty())
            return 0;
        return ankerl::unordered_dense::hash<std::string_view>{}(
            std::string_view(reinterpret_cast<const char *>(v.data()), v.size()));
    }
};

struct UUIDKey
{
    uint64_t hi;
    uint64_t lo;

    bool operator==(const UUIDKey &other) const noexcept
    {
        return hi == other.hi && lo == other.lo;
    }
};

struct UUIDHash
{
    size_t operator()(const UUIDKey &k) const noexcept
    {
        return k.hi ^ k.lo;
    }
};

using NameMap = ankerl::unordered_dense::map<
    std::vector<uint8_t>,
    std::vector<UUIDKey>,
    ByteVecHash>;

struct Zone
{
    uint32_t id;
    NameMap names;
    uint32_t version;
};

using ZoneMap = ankerl::unordered_dense::map<
    std::vector<uint8_t>,
    std::shared_ptr<Zone>,
    ByteVecHash>;

using Records = ankerl::unordered_dense::map<
    UUIDKey,
    Record,
    UUIDHash>;

extern ZoneMap zones;
extern Records records;