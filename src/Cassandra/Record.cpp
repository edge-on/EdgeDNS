#include "Cassandra/Record.hpp"

std::vector<Records::DNSResponseData> DB::Record::getRecord(std::string zone, std::string name, int type)
{
    std::vector<Records::DNSResponseData> res;

    CassStatement *statement =
        cass_statement_new("SELECT * FROM edgeon.records WHERE zone = ? AND name = ? AND type = ?;", 3);

    cass_statement_bind_string(statement, 0, zone.data());
    cass_statement_bind_string(statement, 1, name.data());
    cass_statement_bind_int16(statement, 2, type);

    CassFuture *future =
        cass_session_execute(Main::cas->session, statement);

    cass_future_wait(future);

    if (cass_future_error_code(future) == CASS_OK)
    {
        const CassResult *result = cass_future_get_result(future);
        CassIterator *iterator = cass_iterator_from_result(result);

        while (cass_iterator_next(iterator))
        {
            Records::DNSResponseData data;

            const CassRow *row = cass_iterator_get_row(iterator);

            const CassValue *ttlVal = cass_row_get_column_by_name(row, "ttl");
            const CassValue *prioVal = cass_row_get_column_by_name(row, "prio");
            const CassValue *dataVal = cass_row_get_column_by_name(row, "value");
            const CassValue *ipGroupVal = cass_row_get_column_by_name(row, "ip_group");
            const CassValue *idVal = cass_row_get_column_by_name(row, "id");
            const CassValue *isGeoVal = cass_row_get_column_by_name(row, "is_geo");

            uint32_t ttl;
            cass_value_get_uint32(ttlVal, &ttl);

            uint32_t prio;
            cass_value_get_uint32(prioVal, &prio);

            const char *value;
            size_t len;
            cass_value_get_string(dataVal, &value, &len);

            cass_bool_t isGeo;
            cass_value_get_bool(isGeoVal, &isGeo);

            data.ttl = ttl;
            data.priority = prio;

            if (isGeo)
                cass_value_get_uuid(ipGroupVal, &data.group_id);
            else
                data.rdata = RData::generateRData(value, type);

            cass_value_get_uuid(idVal, &data.id);
            data.is_geo = isGeo;

            res.emplace_back(data);
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);

    return res;
}

std::vector<IpGroupEntry::IpGroupEntryResponse> DB::Record::getIpGroupEntriesCountryBased(CassUuid groupId, char countryCode[8])
{
    std::vector<IpGroupEntry::IpGroupEntryResponse> res;

    if (!isCountryBasedRecordExist(groupId, countryCode))
    {
        return res;
    }

    CassStatement *statement =
        cass_statement_new("SELECT * FROM edgeon.ip_group_entries WHERE group_id = ? AND location_code = ?;", 2);

    cass_statement_bind_uuid(statement, 0, groupId);
    cass_statement_bind_string(statement, 1, countryCode);

    CassFuture *future =
        cass_session_execute(Main::cas->session, statement);

    cass_future_wait(future);

    if (cass_future_error_code(future) == CASS_OK)
    {
        const CassResult *result = cass_future_get_result(future);
        CassIterator *iterator = cass_iterator_from_result(result);

        while (cass_iterator_next(iterator))
        {
            IpGroupEntry::IpGroupEntryResponse data;

            const CassRow *row = cass_iterator_get_row(iterator);

            const CassValue *ipVal = cass_row_get_column_by_name(row, "ip");
            const CassValue *prioVal = cass_row_get_column_by_name(row, "priority");

            const char *ip;
            size_t len;
            cass_value_get_string(ipVal, &ip, &len);

            uint32_t priority;
            cass_value_get_uint32(prioVal, &priority);

            data.ip = RData::generateRData(ip, 1);
            data.priority = priority;

            res.emplace_back(data);
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);

    return res;
}

bool DB::Record::isCountryBasedRecordExist(CassUuid groupId, char countryCode[8])
{
    bool exists = false;
    CassStatement *statement = cass_statement_new("SELECT COUNT(*) as totalCount FROM edgeon.ip_group_entries WHERE group_id = ? AND location_code = ?;", 2);

    cass_statement_bind_uuid(statement, 0, groupId);
    cass_statement_bind_string(statement, 1, countryCode);

    CassFuture *future = cass_session_execute(Main::cas->session, statement);
    cass_future_wait(future);

    if (cass_future_error_code(future) == CASS_OK)
    {
        const CassResult *result = cass_future_get_result(future);
        const CassRow *row = cass_result_first_row(result);

        if (row)
        {
            const CassValue *countVal = cass_row_get_column_by_name(row, "totalCount");
            cass_int64_t count;
            if (cass_value_get_int64(countVal, &count) == CASS_OK)
            {
                exists = (count > 0);
            }
        }
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);

    return exists;
}