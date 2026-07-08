#pragma once

#include <sys/socket.h>
#include <sys/un.h>

#include <liburing.h>

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

#include <liburing/io_uring.h>

#include "Global/Static.hpp"
#include "DNS/Proxy/Proxy.hpp"
#include "Cassandra/Record.hpp"
#include "Core/Thread/Operational.hpp"

#include "Core/Gen/Gen.hpp"

#include "Core/Uring/Pipeline.hpp"

class Core
{
public:
    Core();
    ~Core();

    void write16(std::vector<uint8_t> &buf, uint16_t value);
    void write32(std::vector<uint8_t> &buf, uint32_t value);

    int makeNonBlocking(int sfd);

    void start();

    void worker(int th);

    void initUDP(Gen::Thread &th);
    void initTCP(Gen::Thread &th);

    void handleTCP(Gen::Connection &conn, Gen::Thread &th);
    void writeTCP(Gen::Connection &conn, Gen::Thread &th);

    std::vector<uint8_t> handle(uint8_t *buffer, bool is_tcp, uint32_t ip, char *ip_str, Gen::Thread &thread);

    std::atomic<uint32_t> g_second;
    uint32_t now();
    void start_clock_thread();

private:
    int PORT = 53;

    bool isLogging = false;
    bool rateLimiting = true;

    int threadCount = 1;
    std::vector<std::thread> threads;

    uint32_t threshold = 200;

    std::mutex coutMutex;
};