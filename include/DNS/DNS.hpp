#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <ankerl/unordered_dense.h>

class DNS {
public:
    struct Record {
        uint16_t type;
        uint32_t ttl;
        std::vector<uint8_t> rdata;
    };

    using ByteVec = std::vector<uint8_t>;

    struct ByteVecHash {
        uint64_t operator()(ByteVec const& v) const noexcept {
            return ankerl::unordered_dense::hash<std::string_view>{}(
                std::string_view(reinterpret_cast<const char*>(v.data()), v.size())
            );
        }
    };

    using NameMap = ankerl::unordered_dense::map<
        ByteVec,
        std::vector<Record>,
        ByteVecHash
    >;

    using ZoneMap = ankerl::unordered_dense::map<
        std::string,
        NameMap
    >;

    static ZoneMap zones;
};