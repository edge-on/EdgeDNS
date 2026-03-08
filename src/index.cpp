#include "index.hpp"

Cassandra *Main::cas = nullptr;

int main()
{
    Main::cas = new Cassandra();

    if (Main::cas->connect())
    {
        const CassResult *result = Main::cas->execute("SELECT * FROM edgeon.records;");
        CassIterator *iterator = cass_iterator_from_result(result);

        while (cass_iterator_next(iterator))
        {
            const CassRow *row = cass_iterator_get_row(iterator);

            const CassValue *idVal = cass_row_get_column_by_name(row, "id");
            const CassValue *zoneVal = cass_row_get_column_by_name(row, "zone");
            const CassValue *nameVal = cass_row_get_column_by_name(row, "name");
            const CassValue *typeVal = cass_row_get_column_by_name(row, "type");
            const CassValue *ttlVal = cass_row_get_column_by_name(row, "ttl");
            const CassValue *valueVal = cass_row_get_column_by_name(row, "value");

            const CassValue *versionVal = cass_row_get_column_by_name(row, "version");

            CassUuid version;
            cass_value_get_uuid(versionVal, &version);

            const char *zoneStr;
            size_t zoneLen;
            cass_value_get_string(zoneVal, &zoneStr, &zoneLen);
            std::string zone(zoneStr, zoneLen);

            std::vector<uint8_t> zoneWire = Utils::Vector::stringToWire(std::string(zoneStr, zoneLen), true);

            const char *name;
            size_t nameSize;
            cass_value_get_string(nameVal, &name, &nameSize);

            std::vector<uint8_t> nameWire = Utils::Vector::stringToWire(std::string(name, nameSize), true);

            cass_int16_t type;
            cass_value_get_int16(typeVal, &type);

            cass_int32_t ttl;
            cass_value_get_int32(ttlVal, &ttl);

            const char *rdata;
            size_t rdataSize;
            cass_value_get_string(valueVal, &rdata, &rdataSize);

            std::string rdataStr(rdata, rdataSize);

            std::vector<uint8_t> rdataWire = RData::generateRData(rdata, type);

            Record record;
            record.type = static_cast<uint16_t>(type);
            record.ttl = static_cast<uint32_t>(ttl);
            record.rdata = std::move(rdataWire);

            auto [it, inserted] = zones.try_emplace(zoneWire);

            if (!it->second)
            {
                it->second = std::make_shared<Zone>();
                it->second->id = Main::next_zone_id++;

                if (it->second->version.time_and_version < version.time_and_version)
                {
                    it->second->version = version;
                }
            }

            CassUuid uuid;
            cass_value_get_uuid(idVal, &uuid);

            UUIDKey key = DNS::uuidToKey(uuid);

            records[key] = std::move(record);

            it->second->names[nameWire][type].push_back(std::move(key));
        }

        cass_iterator_free(iterator);

        EoD *eod = new EoD();
        eod->start();
    }

    return 0;
}