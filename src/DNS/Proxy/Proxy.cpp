#include "DNS/Proxy/Proxy.hpp"

void Proxy::reloadProxyGroup()
{
    CassStatement *statement =
        cass_statement_new("SELECT * FROM edgeon.settings WHERE type = ?;", 1);

    cass_statement_bind_string(statement, 0, "proxy_group_id");

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

            const CassValue *typeVal = cass_row_get_column_by_name(row, "type");
            const CassValue *valueVal = cass_row_get_column_by_name(row, "value");

            /* ----- TYPE ----- */
            const char *typeStr;
            size_t typeLen;
            cass_value_get_string(typeVal, &typeStr, &typeLen);

            std::string type(typeStr, typeLen);

            /* ----- VERSION ----- */
            const char *valueStr;
            size_t valueLen;
            cass_value_get_string(valueVal, &valueStr, &valueLen);

            std::string value(valueStr, valueLen);

            cass_uuid_from_string(valueStr, &Proxy::proxy_group_id);
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);

    std::cout << "Proxy Group Loaded!" << std::endl;
}

CassUuid Proxy::proxy_group_id;