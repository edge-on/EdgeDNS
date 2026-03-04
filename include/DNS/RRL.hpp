#pragma once

#include <cstdint>

struct RRLKey
{
    uint64_t prefix; // IPv4/24 and IPv6/64 (prefix)
    uint32_t zone_id;
    uint8_t rcode;

    bool operator==(RRLKey const &other) const noexcept
    {
        return prefix == other.prefix && zone_id == other.zone_id && rcode == other.rcode;
    };
};

struct RRLBucket
{
    uint32_t window_start = 0;
    uint32_t responses;
    uint32_t slip_counter = 2;
    uint32_t last_seen;
};

struct RRLKeyHash
{
    uint64_t operator()(RRLKey const &k) const noexcept
    {
        uint64_t h = k.prefix;
        h ^= uint64_t(k.zone_id) * 0x9E3779B185EBCA87ULL;
        h ^= uint64_t(k.rcode) << 56;
        return h;
    }
};