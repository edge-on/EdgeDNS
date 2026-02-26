#pragma once

#include <sys/socket.h>
#include <sys/epoll.h>

#include <unistd.h>

#include <arpa/inet.h>

#include <iostream>
#include <iomanip>
#include <vector>

class EoD
{
public:
    EoD();
    ~EoD();

    void start();

    struct Connection
    {
        
    };
    

private:
    int eod_port = 8901;

    int epoll_fd;
    int eod_fd;

    int max_event = 10;
};
