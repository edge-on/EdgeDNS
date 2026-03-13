#include "Core/RData/RData.hpp"

std::vector<uint8_t> RData::generateRData(std::string data, int type)
{
    std::vector<uint8_t> rdataWire;

    if (type == 6)
    {
        int sindex = 0;
        for (auto &p : Utils::String::splitBySpace(data))
        {
            if (sindex <= 1)
            {
                auto w = Utils::Vector::stringToWire(p, true);
                rdataWire.insert(rdataWire.end(), w.begin(), w.end());
            }
            else
            {
                uint32_t val = std::stoul(p);
                auto w = Utils::Vector::toBE32(val);
                rdataWire.insert(rdataWire.end(), w.begin(), w.end());
            }
            sindex++;
        }
    }
    else if (type == 1)
    {
        rdataWire = Utils::Vector::ipv4ToWire(data);
    }
    else if(type == 16) {
        rdataWire = Utils::Vector::txtToWire(data);
    }
    else
    {
        rdataWire = Utils::Vector::stringToWire(data, true);
    }

    return rdataWire;
}