#pragma once

#include <sys/socket.h>
#include <sys/epoll.h>

#include <unistd.h>

#include <arpa/inet.h>

#include <iostream>
#include <iomanip>
#include <vector>

#include <fcntl.h>

#include <unordered_map>

class EoD
{
public:
    EoD();
    ~EoD();

    struct Connection
    {
        int fd;
    };

    std::unordered_map<int, Connection> connections;

    void write16(std::vector<uint8_t> &buf, uint16_t value)
    {
        uint16_t net = htons(value);
        uint8_t *p = (uint8_t *)&net;
        buf.push_back(p[0]);
        buf.push_back(p[1]);
    }

    void write32(std::vector<uint8_t> &buf, uint32_t value)
    {
        uint32_t net = htonl(value);
        uint8_t *p = (uint8_t *)&net;
        buf.push_back(p[0]);
        buf.push_back(p[1]);
        buf.push_back(p[2]);
        buf.push_back(p[3]);
    }

    int makeNonBlocking(int sfd)
    {
        int flags = fcntl(sfd, F_GETFL, 0);
        if (flags == -1)
            return -1;
        flags |= O_NONBLOCK;
        return fcntl(sfd, F_SETFL, flags);
    }

    void start();

    void initUDP();
    void handleUDP();

    void initTCP();
    void handleTCP(Connection &conn);

private:
    int eod_port = 8901;

    int epoll_fd;

    int eod_udp_fd;
    int eod_tcp_fd;

    int max_event = 10;
};
