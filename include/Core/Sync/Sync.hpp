#pragma once

#include <sys/socket.h>
#include <arpa/inet.h>
#include <liburing.h>
#include <stdio.h>

#include "Utils/Socket.hpp"

class Sync
{
public:
    static int initSync(int port);
};