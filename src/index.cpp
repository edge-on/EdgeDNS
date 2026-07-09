#include "index.hpp"

Cassandra *Main::cas = nullptr;
Mmap *Main::recordsMap = nullptr;

int main()
{
    Static::dns->init();

    Main::recordsMap = new Mmap();
    Main::recordsMap->init("dnsRecords.bin");

    Main::cas = new Cassandra();
    if (Main::cas->connect())
    {
        Core *core = new Core();
        core->start();
    }

    return 0;
}