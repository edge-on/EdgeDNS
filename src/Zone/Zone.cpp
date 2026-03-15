#include "Zone/Domain.hpp"

bool Domain::zoneIsExist(std::string zone)
{
    bool isExist = false;

    CassStatement *statement =
        cass_statement_new("SELECT domain FROM edgeon.domains_by_name WHERE domain = ?;", 1);

    cass_statement_bind_string(statement, 0, zone.c_str());

    CassFuture *future =
        cass_session_execute(Main::cas->session, statement);

    cass_future_wait(future);

    if (cass_future_error_code(future) == CASS_OK)
    {
        const CassResult *result = cass_future_get_result(future);

        CassIterator *iterator = cass_iterator_from_result(result);
        while (cass_iterator_next(iterator))
        {
            isExist = true;

            break;
        }

        cass_iterator_free(iterator);
        cass_result_free(result);
    }

    cass_future_free(future);
    cass_statement_free(statement);

    return isExist;
}