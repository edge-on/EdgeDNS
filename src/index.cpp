#include "index.hpp"

Cassandra *Main::cas = nullptr;

CassUuid Main::proxyId;

Records::Mmap *Main::recordsMap = nullptr;
IpGroupEntry::Mmap *Main::ipGroupMap = nullptr;
System::Mmap *Main::systemMap = nullptr;

int main()
{
    if (std::filesystem::remove("local/dnsRecords.bin"))
        std::cout << "Records Flushed Successfully!" << std::endl;

    if (std::filesystem::remove("local/ipGroups.bin"))
        std::cout << "Ip Group Entries Flushed Successfully!" << std::endl;

    cass_uuid_from_string("a8b87013-9ff1-4831-859d-c2d3543562d7", &Main::proxyId);

    Static::dns->init();

    Main::recordsMap = new Records::Mmap();
    Main::recordsMap->init("local/dnsRecords.bin");

    Main::ipGroupMap = new IpGroupEntry::Mmap();
    Main::ipGroupMap->init("local/ipGroups.bin");

    Main::systemMap = new System::Mmap();
    Main::systemMap->init("local/system.meta");

    Main::cas = new Cassandra();
    if (Main::cas->connect())
    {
        Core *core = new Core();
        core->start();
    }

    return 0;
}