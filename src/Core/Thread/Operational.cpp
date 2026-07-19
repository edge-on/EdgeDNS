#include "Core/Thread/Operational.hpp"

std::list<Operational::Record> Operational::recordQueue;
std::list<Operational::Entry> Operational::entryQueue;

void Operational::addQueueForRecord(const uint8_t *name, size_t nameLen, int qtype, Records::DNSResponseData req)
{
    Operational::Record record;
    record.name.assign(name, name + nameLen);

    record.qtype = qtype;
    record.ttl = req.ttl;
    record.prio = req.priority;
    record.val = req.rdata;
    record.groupId = req.group_id;
    record.id = req.id;
    record.isGeo = req.is_geo;

    record.type = ADD;

    recordQueue.emplace_back(std::move(record));
}

void Operational::addQueueForEntry(CassUuid groupId, char countryCode[8], IpGroupEntry::IpGroupEntryResponse req)
{
    Operational::Entry entry;
    entry.val = req.ip;
    entry.priority = req.priority;
    entry.groupId = groupId;
    entry.id = req.id;
    memcpy(entry.countryCode, countryCode, 8);

    entry.type = ADD;

    entryQueue.emplace_back(std::move(entry));
}

void Operational::queueLifeCycle()
{
    while (true)
    {
        if (recordQueue.empty() && entryQueue.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (!recordQueue.empty())
        {
            Operational::Record rec;
            rec = std::move(recordQueue.front());
            recordQueue.pop_front();

            if (rec.type == QueueType::ADD)
                Main::recordsMap->append_record(rec.name, rec.qtype, rec.ttl, rec.prio, rec.groupId, rec.id, rec.isGeo, rec.isProxy, rec.val);
        }

        if (!entryQueue.empty())
        {
            Operational::Entry entry;
            entry = std::move(entryQueue.front());
            entryQueue.pop_front();

            if (entry.type == QueueType::ADD)
                Main::ipGroupMap->append_record(entry.groupId, entry.id, entry.countryCode, entry.val, entry.priority);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}