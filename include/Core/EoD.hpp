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
#include <ankerl/unordered_dense.h>

#include <set>

#include <atomic>

#include "DNS/DNS.hpp"
#include "DNS/RRL.hpp"
#include "DNS/Zone.hpp"

#include "Core/IPC/IPC.hpp"

#include "Global/Static.hpp"

class EoD
{
public:
    EoD();
    ~EoD();

    struct Connection
    {
        int fd;

        uint32_t ip;
        char *ip_str;

        std::vector<uint8_t> readBuffer;
        std::vector<uint8_t> writeBuffer;
        uint16_t expectedLength = 0;
    };

    struct Thread
    {
        std::thread::id id;

        int epoll_fd;

        int eod_udp_fd;
        int eod_tcp_fd;

        std::unordered_map<int, Connection> connections;

        ankerl::unordered_dense::map<RRLKey, RRLBucket, RRLKeyHash> rrlBuckets;
    };

    std::unordered_map<int, Thread> activeThreads;

    void write16(std::vector<uint8_t> &buf, uint16_t value);
    void write32(std::vector<uint8_t> &buf, uint32_t value);

    int makeNonBlocking(int sfd);

    void start();

    void worker(int th);

    void initUDP(Thread &th);
    void handleUDP(Thread &th);

    void initTCP(Thread &th);
    void handleTCP(Connection &conn, Thread &th);
    void writeTCP(Connection &conn, Thread &th);

    void initIPC();
    void handleIPC(int fd);

    void enableWrite(int fd, int epoll_fd);
    void disableWrite(int fd, int epoll_fd);

    std::vector<uint8_t> handle(uint8_t buffer[4096], bool is_tcp, uint32_t ip, char *ip_str, Thread &thread);

    std::atomic<uint32_t> g_second;
    uint32_t now();
    void start_clock_thread();

private:
    int eod_port = 53;

    int max_event = 10;

    bool is_logging = false;
    bool is_rrl = true;

    int eod_ipc_fd;

    int threadCount = 1;
    std::vector<std::thread> threads;

    uint32_t threshold = 200;

    std::mutex cout_mutex;
};