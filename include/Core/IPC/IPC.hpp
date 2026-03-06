#pragma once

class IPC
{
public:
    enum Commands {
        RELOAD = 0x10,
        INCREMENTAL = 0x11,
        FAILURE = 0x98,
        DONE = 0x99
    };

private:
};