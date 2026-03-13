#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>

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

        static std::vector<uint8_t> stringToWire(const std::string &input, const bool &zero)
        {
            std::vector<uint8_t> wire;

            std::string domain = input;

            std::transform(domain.begin(), domain.end(), domain.begin(),
                           [](unsigned char c)
                           { return std::tolower(c); });

            if (!domain.empty() && domain.back() == '.')
                domain.pop_back();

            size_t pos = 0;

            while (pos < domain.size())
            {
                size_t dot = domain.find('.', pos);
                if (dot == std::string::npos)
                    dot = domain.size();

                size_t len = dot - pos;

                /*if (len == 0 || len > 63)
                    throw std::runtime_error("Invalid label length");*/

                wire.push_back(static_cast<uint8_t>(len));

                wire.insert(wire.end(),
                            domain.begin() + pos,
                            domain.begin() + dot);

                pos = dot + 1;
            }

            if (zero)
            {
                wire.push_back(0);
            }

            return wire;
        }

        static std::vector<uint8_t> txtToWire(const std::string &txt)
        {
            std::vector<uint8_t> wire;
            if (txt.size() > 255)
                throw std::runtime_error("TXT too long");

            wire.push_back(static_cast<uint8_t>(txt.size()));
            wire.insert(wire.end(), txt.begin(), txt.end());

            return wire;
        }

        static std::string wireToDomain(const uint8_t *data, size_t len)
        {
            std::string result;
            size_t i = 0;

            while (i < len && data[i] != 0)
            {
                uint8_t labelLen = data[i++];
                if (!result.empty())
                    result += '.';

                result.append(reinterpret_cast<const char *>(data + i), labelLen);
                i += labelLen;
            }

            return result;
        }

        static std::vector<uint8_t> ipv4ToWire(const std::string &ip)
        {
            std::vector<uint8_t> wire;
            std::stringstream ss(ip);
            std::string part;
            while (std::getline(ss, part, '.'))
            {
                int byte = std::stoi(part);
                if (byte < 0 || byte > 255)
                    throw std::runtime_error("Invalid IPv4 part");
                wire.push_back(static_cast<uint8_t>(byte));
            }
            if (wire.size() != 4)
                throw std::runtime_error("Invalid IPv4 address");
            return wire;
        }

        static std::vector<uint8_t> toBE32(uint32_t val)
        {
            return std::vector<uint8_t>{
                (uint8_t)(val >> 24),
                (uint8_t)(val >> 16),
                (uint8_t)(val >> 8),
                (uint8_t)(val)};
        };
    };
} // namespace Utils