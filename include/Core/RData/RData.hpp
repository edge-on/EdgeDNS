#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <sstream>
#include <iostream>

#include "Utils/Vector.hpp"
#include "Utils/String.hpp"

class RData
{
public:
    static std::vector<uint8_t> generateRData(std::string data, int type);
};