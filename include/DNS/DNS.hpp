#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include "index.hpp"

#include "DNS/RRL.hpp"
#include "DNS/Zone.hpp"

class DNS
{
public:
    static void reloadZone(std::string domain);
    static void incrementalReloadZone(std::string domain);
};