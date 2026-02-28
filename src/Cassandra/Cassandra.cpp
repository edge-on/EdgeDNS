#include "Cassandra/Cassandra.hpp"

Cassandra::Cassandra()
{
    dotenv = new Dotenv();
    dotenv->config(".env");
}

Cassandra::~Cassandra()
{
}

bool Cassandra::connect()
{
    const std::string ip = dotenv->map["scylla_host"];
    cass_cluster_set_contact_points(cluster, ip.c_str());

    cass_cluster_set_credentials(cluster, dotenv->map["scylla_username"].c_str(), dotenv->map["scylla_password"].c_str());

    connect_future = cass_session_connect(session, cluster);

    if (cass_future_error_code(connect_future) == CASS_OK)
    {
        return true;
    }
    else
    {
        const char *message;
        size_t message_length;
        cass_future_error_message(connect_future, &message, &message_length);
        fprintf(stderr, "Unable to connect: '%.*s'\n", (int)message_length, message);

        return true;
    }

    cass_future_free(connect_future);
    cass_cluster_free(cluster);
    cass_session_free(session);

    return false;
}

const CassResult* Cassandra::execute(char *query) {
    CassStatement* statement = cass_statement_new(query, 0);
    CassFuture* result_future = cass_session_execute(session, statement);

    if(cass_future_error_code(result_future) == CASS_OK) {
        const CassResult* result = cass_future_get_result(result_future);
        
        cass_statement_free(statement);

        return result;
    } else {
        return NULL;
    }
}

std::string Cassandra::getValue(const CassValue* val) {
    CassValueType type = cass_value_type(val);

    switch (type) {
        case CASS_VALUE_TYPE_TEXT:
        case CASS_VALUE_TYPE_VARCHAR: {
            const char* str;
            size_t len;
            cass_value_get_string(val, &str, &len);
            return std::string(str, len);
        }

        case CASS_VALUE_TYPE_UUID: {
            CassUuid uuid;
            cass_value_get_uuid(val, &uuid);

            char buffer[CASS_UUID_STRING_LENGTH];
            cass_uuid_string(uuid, buffer);
            return std::string(buffer);
        }

        case CASS_VALUE_TYPE_INT: {
            cass_int32_t v;
            cass_value_get_int32(val, &v);
            return std::to_string(v);
        }

        case CASS_VALUE_TYPE_BIGINT: {
            cass_int64_t v;
            cass_value_get_int64(val, &v);
            return std::to_string(v);
        }

        case CASS_VALUE_TYPE_BOOLEAN: {
            cass_bool_t v;
            cass_value_get_bool(val, &v);
            return v ? "true" : "false";
        }

        case CASS_VALUE_TYPE_DOUBLE: {
            cass_double_t v;
            cass_value_get_double(val, &v);
            return std::to_string(v);
        }

        default:
            return "[UNSUPPORTED TYPE]";
    }
}
