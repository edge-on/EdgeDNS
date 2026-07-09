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

            uint32_t ttl;
            cass_value_get_uint32(ttlVal, &ttl);

            uint32_t prio;
            cass_value_get_uint32(prioVal, &prio);

            const char *value;
            size_t len;
            cass_value_get_string(dataVal, &value, &len);

            data.ttl = ttl;
            data.priority = prio;
            data.rdata = RData::generateRData(value, type);

            res.emplace_back(data);
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);

    return res;
}