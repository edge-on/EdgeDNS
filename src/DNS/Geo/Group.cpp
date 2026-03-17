#include "DNS/Geo/Group.hpp"

void Group::initIpGroups()
{
    int count = 0;

    CassStatement *statement =
        cass_statement_new("SELECT * FROM edgeon.ip_groups;", 0);

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

            CassUuid group_id;
            cass_value_get_uuid(groupIdVal, &group_id);

            IpGroup group;
            cass_uuid_from_string("00000000-0000-1000-8080-808080808080", &group.version);

            groups[group_id] = group;

            count++;
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);

    std::cout << count << " Ip Group Loaded" << std::endl;
}

void Group::initIpEntries()
{
    int count = 0;

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
            const char *locationCode;
            cass_value_get_string(locationCodeVal, &locationCode, &lcSize);

            std::string locationCodeStr(locationCode, lcSize);

            size_t ipStrSize;
            const char *ip;
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

            auto &vec = group_entries[group_id][locationCodeStr];

            vec.push_back(id);

            entry.indexInVector = vec.size() - 1;

            if (groups[group_id].version.time_and_version < version.time_and_version)
            {
                groups[group_id].version = version;
            }

            entries[group_id][id] = std::move(entry);

            count++;
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);

    std::cout << count << " Ip Entry Loaded" << std::endl;
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
        entries[g].clear();

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
            const char *locationCode;
            cass_value_get_string(locationCodeVal, &locationCode, &lcSize);

            std::string locationCodeStr(locationCode, lcSize);

            size_t ipStrSize;
            const char *ip;
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

            auto &vec = group_entries[group_id][locationCodeStr];

            vec.push_back(id);

            entry.indexInVector = vec.size() - 1;

            if (groups[group_id].version.time_and_version < version.time_and_version)
            {
                groups[group_id].version = version;
            }

            entries[group_id][id] = std::move(entry);
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);
}

void Group::incrementalReload(CassUuid g)
{
    int count = 0;

    CassStatement *statement =
        cass_statement_new("SELECT * FROM edgeon.ip_group_versions WHERE group_id = ?;", 1);

    cass_statement_bind_uuid(statement, 0, g);

    CassUuid v = groups[g].version;

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

            if (action == 1)
            {
                incrementalRemoved(group_id, entry_id);

                count += 1;
            }

            if (groups[g].version.time_and_version < version.time_and_version)
            {
                groups[g].version = version;
            }
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    count += incrementalAdded(g, v);

    cass_future_free(future);
    cass_statement_free(statement);

    std::cout << count << " Ip Entries Reloaded!" << std::endl;
}

void Group::incrementalRemoved(CassUuid group_id, CassUuid entry_id)
{
    int count = 0;
    auto it = entries.find(group_id);
    if (it == entries.end())
        return;

    auto entry_it = it->second.find(entry_id);
    if (entry_it == it->second.end())
        return;

    const std::string &loc = entry_it->second.countryCode;
    auto &vec = group_entries[group_id][loc];
    size_t idx_to_remove = entry_it->second.indexInVector;

    if (idx_to_remove < vec.size())
    {
        if (idx_to_remove != vec.size() - 1)
        {
            CassUuid last_entry_id = vec.back();

            vec[idx_to_remove] = last_entry_id;

            entries[group_id][last_entry_id].indexInVector = idx_to_remove;
        }

        vec.pop_back();
    }

    entries[group_id].erase(entry_it);
}

int Group::incrementalAdded(CassUuid g, CassUuid v)
{
    int count = 0;

    CassStatement *statement =
        cass_statement_new("SELECT * FROM edgeon.ip_group_entries WHERE group_id = ? AND version > ?;", 2);

    cass_statement_bind_uuid(statement, 0, g);
    cass_statement_bind_uuid(statement, 1, v);

    CassFuture *future =
        cass_session_execute(Main::cas->session, statement);

    cass_future_wait(future);

    if (cass_future_error_code(future) == CASS_OK)
    {
        const CassResult *result = cass_future_get_result(future);

        CassIterator *iterator = cass_iterator_from_result(result);

        group_entries[g].clear();
        entries[g].clear();

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
            const char *locationCode;
            cass_value_get_string(locationCodeVal, &locationCode, &lcSize);

            std::string locationCodeStr(locationCode, lcSize);

            size_t ipStrSize;
            const char *ip;
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

            auto &vec = group_entries[group_id][locationCodeStr];

            vec.push_back(id);

            entry.indexInVector = vec.size() - 1;

            if (groups[group_id].version.time_and_version < version.time_and_version)
            {
                groups[group_id].version = version;
            }

            entries[group_id][id] = std::move(entry);

            count++;
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);

    return count;
}