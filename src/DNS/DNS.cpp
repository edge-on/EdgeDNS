#include "DNS/DNS.hpp"

void DNS::reloadZone(std::string zone)
{
    CassStatement *statement =
        cass_statement_new("SELECT * FROM edgeon.records WHERE zone = ?;", 1);

    cass_statement_bind_string(statement, 0, zone.c_str());

    CassFuture *future =
        cass_session_execute(Main::cas->session, statement);

    cass_future_wait(future);

    if (cass_future_error_code(future) == CASS_OK)
    {
        const CassResult *result = cass_future_get_result(future);

        CassIterator *iterator = cass_iterator_from_result(result);

        auto new_zone = std::make_shared<Zone>();
        new_zone->id = Main::next_zone_id++;

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

            if (new_zone->version.time_and_version < version.time_and_version)
            {
                new_zone->version = version;
            }

            const char *zoneStr;
            size_t zoneLen;
            cass_value_get_string(zoneVal, &zoneStr, &zoneLen);

            std::string zoneName(zoneStr, zoneLen);

            const char *name;
            size_t nameSize;
            cass_value_get_string(nameVal, &name, &nameSize);

            std::vector<uint8_t> nameWire = Utils::Vector::stringToWire(name, true);

            cass_int16_t type;
            cass_value_get_int16(typeVal, &type);

            cass_int32_t ttl;
            cass_value_get_int32(ttlVal, &ttl);

            const char *rdata;
            size_t rdataSize;
            cass_value_get_string(valueVal, &rdata, &rdataSize);

            std::string rdataStr(rdata, rdataSize);

            std::vector<uint8_t> rdataWire = RData::generateRData(rdataStr, type);

            Record record;
            record.type = static_cast<uint16_t>(type);
            record.ttl = static_cast<uint32_t>(ttl);
            record.rdata = std::move(rdataWire);

            CassUuid uuid;
            cass_value_get_uuid(idVal, &uuid);

            UUIDKey key = DNS::uuidToKey(uuid);

            records[key] = std::move(record);
            new_zone->names[nameWire][type].push_back(std::move(key));
        }

        std::vector<uint8_t> zoneWire = Utils::Vector::stringToWire(zone, true);
        zones[zoneWire] = new_zone;

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);
}

int DNS::incrementalReloadZone(std::string zone, CassUuid version)
{
    int count = 0;

    CassStatement *statement =
        cass_statement_new("SELECT * FROM edgeon.versions WHERE zone = ? AND version > ?;", 2);

    cass_statement_bind_string(statement, 0, zone.c_str());
    cass_statement_bind_uuid(statement, 1, version);

    CassFuture *future =
        cass_session_execute(Main::cas->session, statement);

    cass_future_wait(future);

    std::vector<uint8_t> zoneWire = Utils::Vector::stringToWire(zone, true);

    if (cass_future_error_code(future) == CASS_OK)
    {
        const CassResult *result = cass_future_get_result(future);

        CassIterator *iterator = cass_iterator_from_result(result);

        while (cass_iterator_next(iterator))
        {
            const CassRow *row = cass_iterator_get_row(iterator);

            const CassValue *zoneVal = cass_row_get_column_by_name(row, "zone");
            const CassValue *versionVal = cass_row_get_column_by_name(row, "version");
            const CassValue *recordVal = cass_row_get_column_by_name(row, "record_id");
            const CassValue *actionVal = cass_row_get_column_by_name(row, "action");

            /* ----- ZONE ----- */
            const char *zoneStr;
            size_t zoneLen;
            cass_value_get_string(zoneVal, &zoneStr, &zoneLen);

            std::string zoneName(zoneStr, zoneLen);

            /* ----- VERSION ----- */
            CassUuid v;
            cass_value_get_uuid(versionVal, &v);

            /* ----- RECORD ----- */
            CassUuid record_id;
            cass_value_get_uuid(recordVal, &record_id);

            /* ----- ACTION ----- */
            cass_int16_t action;
            cass_value_get_int16(actionVal, &action);

            handleIncrementalZone(zoneWire, v, record_id, action);

            count++;
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);

    int aCount = handleIncrementalReloadZone(zoneWire, version);
    count = count + aCount;

    return count;
}

void DNS::handleIncrementalZone(std::vector<uint8_t> zoneWire, CassUuid version, CassUuid record_id, int action)
{
    if (action == 1)
    {
        auto z_it = zones.find(zoneWire);
        if (z_it != zones.end())
        {
            if (z_it->second->version.time_and_version < version.time_and_version)
            {
                z_it->second->version = version;
            }

            UUIDKey key = DNS::uuidToKey(record_id);

            auto it = records.find(key);
            if (it != records.end())
            {
                records.erase(it);
            }
        }
    }
}

int DNS::handleIncrementalReloadZone(std::vector<uint8_t> zoneWire, CassUuid version)
{
    int count = 0;

    CassStatement *statement =
        cass_statement_new("SELECT * FROM edgeon.records WHERE zone = ? AND version > ?;", 2);

    std::string zoneName = Utils::Vector::wireToDomain(zoneWire.data(), zoneWire.size());

    cass_statement_bind_string(statement, 0, zoneName.c_str());
    cass_statement_bind_uuid(statement, 1, version);

    CassFuture *future =
        cass_session_execute(Main::cas->session, statement);

    cass_future_wait(future);

    auto new_zone = std::make_shared<Zone>();

    if (cass_future_error_code(future) == CASS_OK)
    {
        const CassResult *result = cass_future_get_result(future);

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

            std::vector<uint8_t> rdataWire = RData::generateRData(rdataStr, type);

            Record record;
            record.type = static_cast<uint16_t>(type);
            record.ttl = static_cast<uint32_t>(ttl);
            record.rdata = std::move(rdataWire);

            auto [it, inserted] = zones.try_emplace(zoneWire);

            if (inserted)
            {
                it->second = std::make_shared<Zone>();
                it->second->id = Main::next_zone_id++;

                it->second->version.time_and_version = 0;
                it->second->version.clock_seq_and_node = 0;
            }

            if (it->second->version.time_and_version < version.time_and_version)
            {
                it->second->version = version;
            }

            CassUuid uuid;
            cass_value_get_uuid(idVal, &uuid);

            UUIDKey key = DNS::uuidToKey(uuid);

            records[key] = std::move(record);
            it->second->names[nameWire][type].push_back(std::move(key));

            count++;
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);

    return count;
}