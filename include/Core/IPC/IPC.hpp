#pragma once

class IPC
{
public:
    enum Commands {
        RELOAD = 0x10,
        FAILURE = 0x98,
        DONE = 0x99
    };

private:
};