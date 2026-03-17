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

            const CassValue *idVal = cass_row_get_column_by_name(row, "id");
            const CassValue *groupIdVal = cass_row_get_column_by_name(row, "group_id");
            const CassValue *versionVal = cass_row_get_column_by_name(row, "version");
            const CassValue *locationCodeVal = cass_row_get_column_by_name(row, "location_code");
            const CassValue *ipVal = cass_row_get_column_by_name(row, "ip");
            const CassValue *prioVal = cass_row_get_column_by_name(row, "priority");

            CassUuid id;
            cass_value_get_uuid(idVal, &id);
            
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

            IpGroup group;
            cass_uuid_from_string("00000000-0000-1000-8080-808080808080", &group.version);
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);
}

void Group::initIpEntries()
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

            const CassValue *idVal = cass_row_get_column_by_name(row, "id");
            const CassValue *groupIdVal = cass_row_get_column_by_name(row, "group_id");
            const CassValue *versionVal = cass_row_get_column_by_name(row, "version");
            const CassValue *locationCodeVal = cass_row_get_column_by_name(row, "location_code");
            const CassValue *ipVal = cass_row_get_column_by_name(row, "ip");
            const CassValue *prioVal = cass_row_get_column_by_name(row, "priority");

            CassUuid id;
            cass_value_get_uuid(idVal, &id);

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

            if (groups[group_id].version.time_and_version < version.time_and_version)
            {
                groups[group_id].version = version;
            }

            auto &vec = group_entries[group_id][locationCode];
            auto it = std::lower_bound(vec.begin(), vec.end(), entry, [](const IpEntry &a, const IpEntry &b)
                                       { return a.priority < b.priority; });
            vec.insert(it, id);

            entries[id] = entry;
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);
}

void Group::fullReload(CassUuid g)
{
    CassStatement *statement =
        cass_statement_new("SELECT * FROM edgeon.ip_group_entries WHERE group_id = ?;", 1);

    cass_statement_bind_uuid(statement, 0, g);

    CassFuture *future =
        cass_session_execute(Main::cas->session, statement);

    cass_future_wait(future);

    if (cass_future_error_code(future) == CASS_OK)
    {
        const CassResult *result = cass_future_get_result(future);

        CassIterator *iterator = cass_iterator_from_result(result);

        group_entries[g].clear();

        while (cass_iterator_next(iterator))
        {
            const CassRow *row = cass_iterator_get_row(iterator);

            const CassValue *idVal = cass_row_get_column_by_name(row, "id");
            const CassValue *groupIdVal = cass_row_get_column_by_name(row, "group_id");
            const CassValue *versionVal = cass_row_get_column_by_name(row, "version");
            const CassValue *locationCodeVal = cass_row_get_column_by_name(row, "location_code");
            const CassValue *ipVal = cass_row_get_column_by_name(row, "ip");
            const CassValue *prioVal = cass_row_get_column_by_name(row, "priority");

            CassUuid id;
            cass_value_get_uuid(idVal, &id);

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

            if (groups[group_id].version.time_and_version < version.time_and_version)
            {
                groups[group_id].version = version;
            }

            auto &vec = group_entries[group_id][locationCode];
            auto it = std::lower_bound(vec.begin(), vec.end(), entry, [](const IpEntry &a, const IpEntry &b)
                                       { return a.priority < b.priority; });
            vec.insert(it, id);

            entries[id] = entry;
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);
}

void Group::incrementalReload(CassUuid g)
{
    CassStatement *statement =
        cass_statement_new("SELECT * FROM edgeon.ip_group_versions WHERE group_id = ?;", 1);

    cass_statement_bind_uuid(statement, 0, g);

    CassFuture *future =
        cass_session_execute(Main::cas->session, statement);

    cass_future_wait(future);

    if (cass_future_error_code(future) == CASS_OK)
    {
        const CassResult *result = cass_future_get_result(future);

        CassIterator *iterator = cass_iterator_from_result(result);

        group_entries[g].clear();

        while (cass_iterator_next(iterator))
        {
            const CassRow *row = cass_iterator_get_row(iterator);

            const CassValue *groupIdVal = cass_row_get_column_by_name(row, "group_id");
            const CassValue *versionVal = cass_row_get_column_by_name(row, "version");
            const CassValue *entryIdVal = cass_row_get_column_by_name(row, "entry_id");
            const CassValue *actionVal = cass_row_get_column_by_name(row, "action");

            CassUuid group_id;
            cass_value_get_uuid(groupIdVal, &group_id);

            CassUuid version;
            cass_value_get_uuid(versionVal, &version);

            CassUuid entry_id;
            cass_value_get_uuid(entryIdVal, &entry_id);

            cass_int16_t action;
            cass_value_get_int16(actionVal, &action);

            if(action == 1) {
                // Deleted

            }
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);
}

void Group::incrementalAdded() {

}

void Group::incrementalRemoved() {

}