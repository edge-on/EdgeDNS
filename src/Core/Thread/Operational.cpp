#include "Core/Thread/Operational.hpp"

std::list<Operational::Record> Operational::queue;

void Operational::addQueue(const std::vector<uint8_t> &name, int qtype, int ttl, int prio, const std::vector<uint8_t> &val)
{
    Operational::Record record;
    record.name = name;
    record.qtype = qtype;
    record.ttl = ttl;
    record.prio = prio;
    record.val = val;
    record.type = ADD;

    queue.emplace_back(std::move(record));
}

void Operational::queueLifeCycle()
{
    while (true)
    {
        if (queue.empty())
            continue;

        Operational::Record rec;
        rec = std::move(queue.front());
        queue.pop_front();

        if (rec.type == QueueType::ADD)
            Main::map->append_record(rec.name, rec.qtype, rec.ttl, rec.prio, rec.val);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}