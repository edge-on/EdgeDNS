#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <list>

#include "index.hpp"

class Operational
{
public:
    static void addQueue(const std::vector<uint8_t> &name, int qtype, int ttl, int prio, bool isGeo, const std::vector<uint8_t> &val);
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
        bool isGeo;
        std::vector<uint8_t> val;

        QueueType type;
    } Record;

    static std::list<Record> queue;
};