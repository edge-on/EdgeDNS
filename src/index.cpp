#include "index.hpp"

Cassandra *Main::cas = nullptr;

int main()
{
    Static::dns->init();

    Main::cas = new Cassandra();

    if (Main::cas->connect())
    {
        Core *core = new Core();
        core->start();
    }

    return 0;
}