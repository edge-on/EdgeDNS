#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

namespace Utils
{
    class Vector
    {
    public:
        static size_t getHostWireFromWireNoAlloc(const uint8_t *wire, size_t wire_size,
                                                 uint8_t *out, size_t out_cap)
        {
            if (wire_size == 0 || wire[0] == 0)
                return 0;

            size_t label_indices[16];
            size_t label_count = 0;
            size_t i = 0;

            while (i < wire_size && wire[i] != 0 && label_count < 16)
            {
                label_indices[label_count++] = i;
                i += wire[i] + 1;
            }

            size_t start_idx = 0;
            if (label_count > 2)
            {
                start_idx = label_indices[label_count - 2];
            }

            size_t result_size = wire_size - start_idx;
            if (result_size > out_cap)
                result_size = out_cap;

            memcpy(out, wire + start_idx, result_size);
            return result_size;
        }

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
            size_t pos = 0;
            size_t total_size = txt.size();

            if (total_size == 0)
            {
                wire.push_back(0);
                return wire;
            }

            while (pos < total_size)
            {
                uint8_t chunk_len = static_cast<uint8_t>(std::min<size_t>(255, total_size - pos));

                wire.push_back(chunk_len);

                wire.insert(wire.end(),
                            txt.begin() + pos,
                            txt.begin() + pos + chunk_len);

                pos += chunk_len;
            }

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
                try
                {
                    int byte = std::stoi(part);
                    if (byte < 0 || byte > 255)
                        return {};

                    wire.push_back(static_cast<uint8_t>(byte));
                }
                catch (...)
                {
                    return {};
                }
            }

            if (wire.size() != 4)
                return {};

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

        static std::vector<uint8_t> getHostWireFromWire(const uint8_t *wire, size_t wire_size)
        {
            if (wire_size == 0 || wire[0] == 0)
                return {};

            size_t label_indices[16];
            size_t label_count = 0;
            size_t i = 0;

            while (i < wire_size && wire[i] != 0 && label_count < 16)
            {
                label_indices[label_count++] = i;
                i += wire[i] + 1;
            }

            size_t start_idx = 0;
            if (label_count > 2)
            {
                start_idx = label_indices[label_count - 2];
            }

            size_t result_size = wire_size - start_idx;

            std::vector<uint8_t> result;
            result.reserve(result_size);
            result.assign(wire + start_idx, wire + wire_size);

            return result;
        }
    };
} // namespace Utils