#pragma once

#include <sys/socket.h>
#include <sys/epoll.h>

#include <unistd.h>

#include <arpa/inet.h>

#include <iostream>
#include <iomanip>
#include <vector>

#include <fcntl.h>

#include <thread>

#include <unordered_map>

class EoD
{
public:
    EoD();
    ~EoD();

    struct Connection
    {
        int fd;

        std::vector<uint8_t> readBuffer;
        std::vector<uint8_t> writeBuffer;
        uint16_t expectedLength = 0;
    };

    struct Thread
    {
        int epoll_fd;

        int eod_udp_fd;
        int eod_tcp_fd;

        std::unordered_map<int, Connection> connections;
    };

    std::unordered_map<int, Thread> activeThreads;

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

    void worker(int th);

    void initUDP(Thread &th);
    void handleUDP(Thread &th);

    void initTCP(Thread &th);
    void handleTCP(Connection &conn, Thread &th);
    void writeTCP(Connection &conn, Thread &th);

    void enableWrite(int fd, int epoll_fd);
    void disableWrite(int fd, int epoll_fd);

    std::vector<uint8_t> handle(uint8_t buffer[4096], bool is_tcp);

private:
    int eod_port = 8902;

    int max_event = 10;

    bool is_logging = false;

    int threadCount = 16;
    std::vector<std::thread> threads;
};