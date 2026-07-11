#pragma once

#include <string>
#include <vector>

#include "index.hpp"

#include "Cache/MmapRecords.hpp"

namespace DB
{
    class Record
    {
    public:
        static std::vector<Records::DNSResponseData> getRecord(std::string zone, std::string name, int type);
        static std::vector<IpGroupEntry::IpGroupEntryResponse> getIpGroupEntriesCountryBased(CassUuid groupId, char countryCode[8]);

    private:
        static bool isCountryBasedRecordExist(CassUuid groupId, char countryCode[8]);
    };
} // namespace Cassandra