#pragma once

#include <cassandra.h>

#include "index.hpp"

class Proxy
{
public:
    static void reloadProxyGroup();

    static CassUuid proxy_group_id;
};