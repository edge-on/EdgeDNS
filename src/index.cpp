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

            const CassValue *zoneVal = cass_row_get_column_by_name(row, "zone");
            const CassValue *nameVal = cass_row_get_column_by_name(row, "name");
            const CassValue *typeVal = cass_row_get_column_by_name(row, "type");
            const CassValue *ttlVal = cass_row_get_column_by_name(row, "ttl");
            const CassValue *valueVal = cass_row_get_column_by_name(row, "value");

            const CassValue *versionVal = cass_row_get_column_by_name(row, "version");

            cass_int32_t version;
            cass_value_get_int32(versionVal, &version);

            const char *zoneStr;
            size_t zoneLen;
            cass_value_get_string(zoneVal, &zoneStr, &zoneLen);
            std::string zone(zoneStr, zoneLen);

            const cass_byte_t *nameBytes;
            size_t nameSize;
            cass_value_get_bytes(nameVal, &nameBytes, &nameSize);

            std::vector<uint8_t> nameWire(nameBytes, nameBytes + nameSize);

            cass_int16_t type;
            cass_value_get_int16(typeVal, &type);

            cass_int32_t ttl;
            cass_value_get_int32(ttlVal, &ttl);

            const cass_byte_t *rdataBytes;
            size_t rdataSize;
            cass_value_get_bytes(valueVal, &rdataBytes, &rdataSize);

            std::vector<uint8_t> rdata(rdataBytes, rdataBytes + rdataSize);

            Record record;
            record.type = static_cast<uint16_t>(type);
            record.ttl = static_cast<uint32_t>(ttl);
            record.rdata = std::move(rdata);

            std::vector<uint8_t> zoneWire = Utils::Vector::stringToWire(zone);

            auto [it, inserted] = zones.try_emplace(zoneWire);

            if (!it->second)
            {
                it->second = std::make_shared<Zone>();
                it->second->id = Main::next_zone_id++;

                if (it->second->version < version)
                {
                    it->second->version = version;
                }
            }

            it->second->names[nameWire].push_back(std::move(record));
        }

        cass_iterator_free(iterator);

        EoD *eod = new EoD();
        eod->start();
    }

    return 0;
}