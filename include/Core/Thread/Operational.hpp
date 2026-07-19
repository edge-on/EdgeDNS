#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <list>

#include "index.hpp"

class Operational
{
public:
    static void addQueueForRecord(const uint8_t *name, size_t nameLen, int qtype, Records::DNSResponseData req);
    static void addQueueForEntry(CassUuid groupId, char countryCode[8], IpGroupEntry::IpGroupEntryResponse entry);

    static void queueLifeCycle();

    typedef enum
    {
        ADD,
        REMOVE,
        UPDATE
    } QueueType;

    typedef struct
    {
        std::vector<uint8_t> name;
        int qtype;
        int ttl;
        int prio;
        CassUuid groupId;
        CassUuid id;
        bool isGeo;
        bool isProxy;
        std::vector<uint8_t> val;

        QueueType type;
    } Record;
    static std::list<Record> recordQueue;

    typedef struct
    {
        CassUuid groupId;
        CassUuid id;
        char countryCode[8];
        std::vector<uint8_t> val;
        int priority;

        QueueType type;
    } Entry;
    static std::list<Entry> entryQueue;
};