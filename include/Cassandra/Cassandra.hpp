#pragma once

#include <cassandra.h>
#include <string>

#include "Dotenv/Dotenv.hpp"

class Cassandra
{
public:
    Cassandra();
    ~Cassandra();

    bool connect();

    const CassResult* execute(char *query);

    std::string getValue(const CassValue* val);
private:
    CassFuture* connect_future = NULL;
    CassCluster* cluster = cass_cluster_new();
    CassSession* session = cass_session_new();

    Dotenv *dotenv;
};