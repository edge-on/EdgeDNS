#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include "index.hpp"

#include "DNS/RRL.hpp"
#include "DNS/Zone.hpp"

#include "Zone/Domain.hpp"

class DNS
{
public:
    static UUIDKey uuidToKey(const CassUuid &u)
    {
        return {u.time_and_version, u.clock_seq_and_node};
    }

    // Full Reload
    static void reloadZone(std::string domain);

    // Incremental Reload
    static int incrementalReloadZone(std::string zone, CassUuid version);
    static int handleIncrementalReloadZone(std::vector<uint8_t> zoneWire, CassUuid version);

private:
    static void handleIncrementalZone(std::vector<uint8_t> zoneWire, CassUuid version, CassUuid record_id, int action);
};