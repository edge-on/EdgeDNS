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
    // Full Reload
    static void reloadZone(std::string domain);

    // Incremental Reload
    static void incrementalReloadZone(std::string domain);
private:
    static void handleIncrementalZone(std::string zoneName, CassUuid version, CassUuid record_id, int action);
};