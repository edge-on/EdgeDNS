#pragma once

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>

#include <unistd.h>

#include <arpa/inet.h>

#include <iostream>
#include <iomanip>
#include <vector>

#include <fcntl.h>

#include <thread>

#include <unordered_map>

#include <atomic>

#include "DNS/DNS.hpp"

class EoD
{
public:
    EoD();
    ~EoD();

    enum ConnType {
        TCP,
        IPC
    };

    struct Connection
    {
        int fd;

        uint32_t ip;

        ConnType type;

        std::vector<uint8_t> readBuffer;
        std::vector<uint8_t> writeBuffer;
        uint16_t expectedLength = 0;
    };

    struct Thread
    {
        int epoll_fd;

        int eod_udp_fd;
        int eod_tcp_fd;

        int eod_ipc_fd;

        std::unordered_map<int, Connection> connections;

        ankerl::unordered_dense::map<DNS::RRLKey, DNS::RRLBucket, DNS::RRLKeyHash> rrlBuckets;
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

    void initIPC(Thread &th);

    void enableWrite(int fd, int epoll_fd);
    void disableWrite(int fd, int epoll_fd);

    std::vector<uint8_t> handle(uint8_t buffer[4096], bool is_tcp, uint32_t ip, Thread &thread);

    std::atomic<uint32_t> g_second;

    uint32_t now()
    {
        return g_second.load(std::memory_order_relaxed);
    }

    void start_clock_thread()
    {
        std::thread([&]
                    {
        while (true) {
            timespec ts;
            clock_gettime(CLOCK_MONOTONIC_COARSE, &ts);
            g_second.store(static_cast<uint32_t>(ts.tv_sec),
                           std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } })
            .detach();
    }

private:
    int eod_port = 8902;

    int max_event = 10;

    bool is_logging = false;
    bool is_rrl = false;

    int threadCount = 1;
    std::vector<std::thread> threads;

    uint32_t threshold = 200;
};