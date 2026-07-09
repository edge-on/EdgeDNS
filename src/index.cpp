#include "index.hpp"

Cassandra *Main::cas = nullptr;
Records::Mmap *Main::recordsMap = nullptr;
System::Mmap *Main::systemMap = nullptr;

int main()
{
    Static::dns->init();

    Main::recordsMap = new Records::Mmap();
    Main::recordsMap->init("local/dnsRecords.bin");

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