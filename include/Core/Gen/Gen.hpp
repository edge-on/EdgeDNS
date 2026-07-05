#pragma once

#include <thread>
#include <unordered_map>
#include <ankerl/unordered_dense.h>
#include <liburing.h>

#include "DNS/RRL.hpp"

#include <list>
#include <netinet/in.h>
#include <arpa/inet.h>

#define QUEUE_DEPTH 8192

class Gen
{
public:
    // Connection
    typedef struct
    {
        int fd;

        uint32_t ip;
        char *ip_str;

        int type;

        std::vector<uint8_t> readBuffer;
        std::vector<uint8_t> writeBuffer;

        struct msghdr msgHdr{};

        uint16_t expectedLength = 0;
    } Connection;

    // Thread
    typedef struct
    {
        std::thread::id id;

        int udpFd;
        int tcpFd;

        struct io_uring ring;

        std::unordered_map<int, Connection> connections;

        ankerl::unordered_dense::map<RRLKey, RRLBucket, RRLKeyHash> rrlBuckets;
    } Thread;

    static std::unordered_map<int, Thread> activeThreads;

    // State Machine
    typedef enum
    {
        STATE_MULTISHOT_ACCEPT,
        STATE_READ,
        STATE_WRITE
    };

    typedef enum
    {
        UDP,
        TCP
    };

    struct Context
    {
        int fd;
        std::vector<uint8_t> writeBuffer;
        sockaddr_storage peerAddr;
        socklen_t peerLen;
        iovec iov;
        msghdr msgHdr;
    };
};