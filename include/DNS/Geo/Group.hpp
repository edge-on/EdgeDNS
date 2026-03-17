#pragma once

#include <cassandra.h>
#include <ankerl/unordered_dense.h>

#include "DNS/Geo/IpGroup.hpp"

#include "index.hpp"

class Group {
public:
    static void initIpGroups();
    static void initIpEntries();

    static void fullReload(CassUuid groupId);
    static void incrementalReload(CassUuid groupId);
    
    static void incrementalRemoved(CassUuid groupId);
    static void incrementalAdded(CassUuid groupId);
};