#pragma once

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

using NameMap = ankerl::unordered_dense::map<
    std::vector<uint8_t>,
    std::vector<Record>,
    ByteVecHash>;

struct Zone
{
    uint32_t id;
    NameMap names;
};

using ZoneMap = ankerl::unordered_dense::map<
    std::vector<uint8_t>,
    std::shared_ptr<Zone>,
    ByteVecHash>;

extern ZoneMap zones;