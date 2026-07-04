#include "index.hpp"

Cassandra *Main::cas = nullptr;
Mmap *Main::map = nullptr;

int main()
{
    Static::dns->init();

    Main::map = new Mmap();
    Main::map->init("edgedns.db");

    Main::cas = new Cassandra();
    if (Main::cas->connect())
    {
        Core *core = new Core();
        core->start();
    }

    return 0;
}