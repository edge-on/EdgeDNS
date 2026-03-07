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

            CassUuid uuid;
            cass_value_get_uuid(idVal, &uuid);

            UUIDKey key = DNS::uuidToKey(uuid);

            records[key] = std::move(record);
            new_zone->names[nameWire].push_back(std::move(key));
        }

        std::vector<uint8_t> zoneWire = Utils::Vector::stringToWire(zone);
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

    std::vector<uint8_t> zoneWire = Utils::Vector::stringToWire(zone);

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
            CassUuid version;
            cass_value_get_uuid(versionVal, &version);

            /* ----- RECORD ----- */
            CassUuid record_id;
            cass_value_get_uuid(recordVal, &record_id);

            /* ----- ACTION ----- */
            cass_int16_t action;
            cass_value_get_int16(actionVal, &action);

            handleIncrementalZone(zoneWire, version, record_id, action);

            count++;
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);

    count = count + handleIncrementalReloadZone(zoneWire, zones[zoneWire]->version);

    return count;
}

void DNS::handleIncrementalZone(std::vector<uint8_t> zoneWire, CassUuid version, CassUuid record_id, int action)
{
    if (action == 1)
    {
        if (zones[zoneWire]->version.time_and_version < version.time_and_version)
        {
            zones[zoneWire]->version = version;
        }

        UUIDKey key = DNS::uuidToKey(record_id);

        auto it = records.find(key);
        if (it != records.end())
        {
            records.erase(it);
        }
    }
}

int DNS::handleIncrementalReloadZone(std::vector<uint8_t> zoneWire, CassUuid version)
{
    int count = 0;

    CassStatement *statement =
        cass_statement_new("SELECT * FROM edgeon.records WHERE zone = ? AND version > ?;", 2);

    cass_statement_bind_string(statement, 0, Utils::Vector::wireToDomain(zoneWire.data(), zoneWire.size()).c_str());
    cass_statement_bind_uuid(statement, 1, version);

    CassFuture *future =
        cass_session_execute(Main::cas->session, statement);

    cass_future_wait(future);

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

            if (zones[zoneWire]->version.time_and_version < version.time_and_version)
            {
                zones[zoneWire]->version = version;

                std::cout << "Here worked" << std::endl;
            }

            const char *zoneStr;
            size_t zoneLen;
            cass_value_get_string(zoneVal, &zoneStr, &zoneLen);

            std::string zoneName(zoneStr, zoneLen);

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

            CassUuid uuid;
            cass_value_get_uuid(idVal, &uuid);

            UUIDKey key = DNS::uuidToKey(uuid);

            records[key] = std::move(record);
            zones[zoneWire]->names[nameWire].push_back(std::move(key));

            count++;
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);

    return count;
}