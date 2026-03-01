#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace Utils
{
    class Vector
    {
    public:
        static std::vector<uint8_t> u64ToVectorBE(uint64_t value)
        {
            std::vector<uint8_t> out;
            out.reserve(8);

            for (int i = 7; i >= 0; --i)
            {
                out.push_back((value >> (i * 8)) & 0xFF);
            }

            return out;
        }

        static std::vector<uint8_t> stringToBytes(const std::string &str)
        {
            return std::vector<uint8_t>(str.begin(), str.end());
        }
    };
} // namespace Utils