#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <liburing.h>
#include <stdio.h>

#include "Utils/Socket.hpp"

#include "Core/Gen/Gen.hpp"

class Sync
{
public:
    static void initSync(int port, Gen::Thread &thread);
};