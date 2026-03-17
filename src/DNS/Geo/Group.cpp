#include "DNS/Geo/Group.hpp"

void Group::initIpGroups()
{
    CassStatement *statement =
        cass_statement_new("SELECT * FROM edgeon.ip_group_entries;", 0);

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

            const CassValue *groupIdVal = cass_row_get_column_by_name(row, "group_id");
            const CassValue *versionVal = cass_row_get_column_by_name(row, "version");
            const CassValue *locationCodeVal = cass_row_get_column_by_name(row, "location_code");
            const CassValue *ipVal = cass_row_get_column_by_name(row, "ip");
            const CassValue *prioVal = cass_row_get_column_by_name(row, "priority");

            CassUuid group_id;
            cass_value_get_uuid(groupIdVal, &group_id);

            CassUuid version;
            cass_value_get_uuid(versionVal, &version);

            size_t lcSize;
            char *locationCode;
            cass_value_get_string(locationCodeVal, &locationCode, &lcSize);

            std::string locationCodeStr(locationCode, lcSize);

            size_t ipStrSize;
            char *ip;
            cass_value_get_string(ipVal, &ip, &ipStrSize);

            std::string ipStr(ip, ipStrSize);

            std::vector<uint8_t> ipWire;
            ipWire = RData::generateRData(ipStr, 1);

            cass_int16_t priority;
            cass_value_get_int16(prioVal, &priority);

            IpEntry entry;
            entry.ip = ipWire;
            entry.countryCode = locationCodeStr;
            entry.priority = priority;

            auto &vec = groups[group_id][locationCode];
            auto it = std::lower_bound(vec.begin(), vec.end(), entry, [](const IpEntry &a, const IpEntry &b)
                                       {
                                           return a.priority < b.priority;
                                       });
            vec.insert(it, entry);
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);
}

void Group::fullReload(CassUuid groupId)
{
}

void Group::incrementalReload(CassUuid groupId)
{
}