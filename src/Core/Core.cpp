#include "Core/Core.hpp"

Core::Core()
{
}

Core::~Core()
{
}

void Core::start()
{
    threadCount = std::thread::hardware_concurrency() / 4;

    start_clock_thread();

    for (int i = 0; i < threadCount; ++i)
    {
        threads.emplace_back(&Core::worker, this, i);
        Gen::activeThreads[i].id = threads[i].get_id();
    }

    std::thread operationalThread(Operational::queueLifeCycle);

    for (auto &t : threads)
    {
        t.join();
    }

    operationalThread.detach();
}

void Core::initUDP(Gen::Thread &thread)
{
    thread.udpFd = socket(AF_INET, SOCK_DGRAM, 0);

    makeNonBlocking(thread.udpFd);

    if (thread.udpFd == 0)
    {
        perror("eod socket");
    }

    int opt = 1;
    setsockopt(thread.udpFd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in eod_addr{};
    eod_addr.sin_family = AF_INET;
    eod_addr.sin_addr.s_addr = INADDR_ANY;
    eod_addr.sin_port = htons(PORT);

    if (bind(thread.udpFd, (sockaddr *)&eod_addr, sizeof(eod_addr)) < 0)
    {
        perror("eod bind");
    }

    Gen::Connection conn;
    conn.fd = thread.udpFd;
    conn.type = Gen::UDP;

    thread.connections[thread.udpFd] = std::move(conn);

    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << "UDP Socket Initalized On Thread " << thread.id << ".\n";
}

void Core::initTCP(Gen::Thread &thread)
{
    thread.tcpFd = socket(AF_INET, SOCK_STREAM, 0);

    makeNonBlocking(thread.tcpFd);

    if (thread.tcpFd == 0)
    {
        perror("eod tcp socket");
    }

    int opt = 1;
    setsockopt(thread.tcpFd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in eod_addr{};
    eod_addr.sin_family = AF_INET;
    eod_addr.sin_addr.s_addr = INADDR_ANY;
    eod_addr.sin_port = htons(PORT);

    if (bind(thread.tcpFd, (sockaddr *)&eod_addr, sizeof(eod_addr)) < 0)
    {
        perror("eod tcp bind");
    }

    if (listen(thread.tcpFd, SOMAXCONN) < 0)
    {
        perror("eod tcp listen");
    }

    Gen::Connection conn;
    conn.fd = thread.tcpFd;
    conn.type = Gen::TCP;

    thread.connections[thread.tcpFd] = std::move(conn);

    std::lock_guard<std::mutex> lock(coutMutex);
    std::cout << "TCP Socket Initalized On Thread " << thread.id << ".\n";
}

void Core::worker(int th)
{
    Gen::Thread &thread = Gen::activeThreads[th];

    struct io_uring *ring = &thread.ring;
    if (io_uring_queue_init(QUEUE_DEPTH, ring, 0) < 0)
    {
        perror("io uring queue init");
        return;
    }

    Pipeline *pipeline = new Pipeline();
    pipeline->init(th);

    initUDP(thread);
    initTCP(thread);

    pipeline->queueRead(thread.connections[thread.udpFd]);
    pipeline->queueMultishotAccept(thread.tcpFd);

    io_uring_submit(ring);

    while (true)
    {
        struct io_uring_cqe *cqe;
        int ret = io_uring_wait_cqe(ring, &cqe);
        if (ret < 0)
            break;

        uint64_t data = (uint64_t)io_uring_cqe_get_data(cqe);

        if (data & (1ULL << 63))
        {
            Gen::Context *ctx = (Gen::Context *)(data & ~(1ULL << 63));
            if (cqe->res < 0)
                std::cerr << "sendmsg failed: " << strerror(-cqe->res) << std::endl;
            delete ctx;
            io_uring_cqe_seen(ring, cqe);
            continue;
        }

        int fd = (int)(data & 0xFFFFFFFF);
        int type = (int)(data >> 32);

        int res = cqe->res;
        bool hasMore = cqe->flags & IORING_CQE_F_MORE;
        io_uring_cqe_seen(ring, cqe);

        if (res < 0)
        {
            // Close
            auto it = thread.connections.find(fd);
            if (it != thread.connections.end())
            {
                thread.connections.erase(fd);
                close(fd);
            }

            // Multishot Accept
            if (type == Gen::STATE_MULTISHOT_ACCEPT)
            {
                pipeline->queueMultishotAccept(fd);
                io_uring_submit(ring);
            }

            continue;
        }

        if (type == Gen::STATE_MULTISHOT_ACCEPT)
        {
            int t = Gen::TCP;
            int clientFd = res;

            Gen::Connection conn;
            conn.fd = clientFd;
            conn.type = t;

            struct sockaddr_storage addr;
            socklen_t len = sizeof(addr);

            if (getpeername(clientFd, (struct sockaddr *)&addr, &len) == 0)
            {
                char ip_str[INET6_ADDRSTRLEN];

                if (addr.ss_family == AF_INET)
                {
                    struct sockaddr_in *addr_ipv4 = (struct sockaddr_in *)&addr;
                    inet_ntop(AF_INET, &(addr_ipv4->sin_addr), ip_str, INET_ADDRSTRLEN);
                }
                else if (addr.ss_family == AF_INET6)
                {
                    struct sockaddr_in6 *addr_ipv6 = (struct sockaddr_in6 *)&addr;
                    inet_ntop(AF_INET6, &(addr_ipv6->sin6_addr), ip_str, INET6_ADDRSTRLEN);
                }

                conn.ip = ((sockaddr_in *)&addr)->sin_addr.s_addr;
                conn.ip_str = ip_str;
            }

            conn.type = Gen::TCP;

            thread.connections[clientFd] = std::move(conn);

            pipeline->queueRead(thread.connections[clientFd]);
            io_uring_submit(ring);

            continue;
        }

        auto it = thread.connections.find(fd);
        if (it == thread.connections.end())
            continue;

        Gen::Connection &conn = it->second;

        switch (type)
        {
        case Gen::STATE_READ:
        {
            int buf_id = cqe->flags >> IORING_CQE_BUFFER_SHIFT;
            char *raw = pipeline->pool->getBufferAddress(buf_id);
            size_t buf_size = pipeline->pool->getBufferSize();

            if (cqe->res < 0)
            {
                std::cerr << "recvmsg failed: " << strerror(-cqe->res) << " - FD: " << conn.fd << " Res: " << res << std::endl;
                pipeline->pool->releaseBuffer(buf_id);
                break;
            }

            struct io_uring_recvmsg_out *o = io_uring_recvmsg_validate(raw, cqe->res, &conn.msgHdr);
            if (!o)
            {
                pipeline->pool->releaseBuffer(buf_id);
                break;
            }

            void *payload = io_uring_recvmsg_payload(o, &conn.msgHdr);
            size_t payload_len = io_uring_recvmsg_payload_length(o, cqe->res, &conn.msgHdr);

            uint8_t queryBuf[512];
            size_t queryLen = std::min(payload_len, sizeof(queryBuf));
            memcpy(queryBuf, payload, queryLen);

            pipeline->pool->releaseBuffer(buf_id);

            switch (conn.type)
            {
            case Gen::TCP:

                conn.writeBuffer = handle(queryBuf, true, conn.ip, conn.ip_str, thread);

                pipeline->queueWriteTcp(conn);
                io_uring_submit(ring);
                break;
            case Gen::UDP:
            {
                sockaddr_storage peerAddr;
                memcpy(&peerAddr, io_uring_recvmsg_name(o), o->namelen);
                socklen_t peerLen = o->namelen;

                char ipStr[INET6_ADDRSTRLEN] = {0};
                if (peerAddr.ss_family == AF_INET)
                    inet_ntop(AF_INET, &((sockaddr_in *)&peerAddr)->sin_addr, ipStr, sizeof(ipStr));
                else if (peerAddr.ss_family == AF_INET6)
                    inet_ntop(AF_INET6, &((sockaddr_in6 *)&peerAddr)->sin6_addr, ipStr, sizeof(ipStr));

                Gen::Context *ctx = new Gen::Context();
                ctx->fd = conn.fd;
                memcpy(&ctx->peerAddr, &peerAddr, peerLen);
                ctx->peerLen = peerLen;

                ctx->writeBuffer = handle(queryBuf, false, ((sockaddr_in *)&peerAddr)->sin_addr.s_addr, ipStr, thread);

                ctx->iov.iov_base = ctx->writeBuffer.data();
                ctx->iov.iov_len = ctx->writeBuffer.size();

                pipeline->queueWriteUdp(ctx);
                io_uring_submit(ring);
                break;
            }
            }
            break;
        }

        case Gen::STATE_WRITE:
        {
            break;
        }
        }
    }
}

void Core::handleTCP(Gen::Connection &conn, Gen::Thread &thread)
{
    uint8_t temp[4096];
    ssize_t n = read(conn.fd, temp, sizeof(temp));

    if (n <= 0)
        return;

    conn.readBuffer.insert(conn.readBuffer.end(), temp, temp + n);

    while (true)
    {
        if (conn.expectedLength == 0)
        {
            if (conn.readBuffer.size() < 2)
                return;

            conn.expectedLength =
                (conn.readBuffer[0] << 8) |
                (conn.readBuffer[1]);

            conn.readBuffer.erase(conn.readBuffer.begin(),
                                  conn.readBuffer.begin() + 2);
        }

        if (conn.readBuffer.size() < conn.expectedLength)
            return;

        std::vector<uint8_t> dnsPacket(
            conn.readBuffer.begin(),
            conn.readBuffer.begin() + conn.expectedLength);

        conn.readBuffer.erase(conn.readBuffer.begin(),
                              conn.readBuffer.begin() + conn.expectedLength);

        conn.expectedLength = 0;

        auto response = handle(dnsPacket.data(), true, conn.ip, conn.ip_str, thread);

        uint16_t len = htons(response.size());

        conn.writeBuffer.insert(conn.writeBuffer.end(),
                                (uint8_t *)&len,
                                (uint8_t *)&len + 2);

        conn.writeBuffer.insert(conn.writeBuffer.end(),
                                response.begin(),
                                response.end());
    }
}

void Core::writeTCP(Gen::Connection &conn, Gen::Thread &thread)
{
    while (!conn.writeBuffer.empty())
    {
        ssize_t sent = send(conn.fd,
                            conn.writeBuffer.data(),
                            conn.writeBuffer.size(),
                            0);

        if (sent < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            return;
        }

        conn.writeBuffer.erase(
            conn.writeBuffer.begin(),
            conn.writeBuffer.begin() + sent);
    }
}

std::vector<uint8_t> Core::handle(uint8_t *buffer, bool is_tcp, uint32_t ip, char *ip_str, Gen::Thread &thread)
{
    if (isLogging)
    {
        std::cout << (is_tcp ? "TCP " : "UDP ") << "Request" << std::endl;
    }

    bool truncated = false;

    size_t offset = is_tcp ? 2 : 0;

    auto read16 = [&](void)
    {
        uint16_t v = (buffer[offset] << 8) | buffer[offset + 1];
        offset += 2;
        return v;
    };

    auto write16 = [&](std::vector<uint8_t> &out, uint16_t v)
    {
        out.push_back((v >> 8) & 0xFF);
        out.push_back(v & 0xFF);
    };

    auto write32 = [&](std::vector<uint8_t> &out, uint32_t v)
    {
        out.push_back((v >> 24) & 0xFF);
        out.push_back((v >> 16) & 0xFF);
        out.push_back((v >> 8) & 0xFF);
        out.push_back(v & 0xFF);
    };

    // ---------------- HEADER ----------------
    uint16_t transaction_id = read16();
    uint16_t flags = read16();
    uint16_t qdcount = read16();
    read16(); // ancount
    read16(); // nscount
    read16(); // arcount

    // ---------------- READ QNAME ----------------
    std::vector<uint8_t> nameWire;
    nameWire.reserve(256);

    size_t name_start = offset;

    while (buffer[offset] != 0)
    {
        uint8_t len = buffer[offset++];
        offset += len;
    }

    offset++; // root

    nameWire.insert(nameWire.end(),
                    buffer + name_start,
                    buffer + offset);

    uint16_t qtype = read16();
    uint16_t qclass = read16();

    size_t question_end = offset;

    // ---------------- READ ADDITIONAL ----------------
    // NAME
    offset++;

    uint16_t edns_type = read16();
    uint16_t edns_class = read16();

    // TTL
    read16();
    read16();

    // RDLEN
    read16();

    // ---------------- FIND ZONE (Longest suffix) ----------------
    for (auto &byte : nameWire)
    {
        if (byte >= 'A' && byte <= 'Z')
        {
            byte += 32;
        }
    }

    std::vector<uint8_t> response;
    response.reserve(512);

    uint16_t response_flags = 0;
    uint16_t anc = 0;

    std::vector<Records::DNSResponseData> matched_records;
    bool cacheHit = Main::recordsMap->get_record(nameWire, qtype, matched_records);

    std::vector<uint8_t> zoneData;
    if (!cacheHit || rateLimiting)
    {
        zoneData = Utils::Vector::getHostWireFromWire(nameWire.data(), nameWire.size());
    }

    if (!cacheHit)
    {
        // Go To DB, Take the data and append to mmap
        std::string name = Utils::Vector::wireToDomain(nameWire.data(), nameWire.size());
        std::string zone = Utils::Vector::wireToDomain(zoneData.data(), zoneData.size());

        matched_records = DB::Record::getRecord(zone, name, qtype);
    }

    // ---------------- BUILD RESPONSE ----------------

    /*
    0 → NOERROR
    1 → FORMERR
    2 → SERVFAIL
    3 → NXDOMAIN
    4 → NOTIMP
    5 → REFUSED
    */

    write16(response, transaction_id);

    response_flags |= 0x8000;           // QR
    response_flags |= (flags & 0x0100); // RD mirror
    response_flags |= 0x0400;           // AA

    if (qclass != 1)
    {
        response_flags |= 0x0004; // NOTIMP
    }

    if (qdcount != 1)
    {
        response_flags |= 0x0001; // FORMER
    }

    if (qtype != 1)
    {
        response_flags |= 0x0005; // REFUSED
    }

    write16(response, response_flags);
    write16(response, 1); // QDCOUNT
    write16(response, 0); // ANCOUNT placeholder
    write16(response, 0); // NSCOUNT
    write16(response, 0); // ARCOUNT

    // copy question
    response.insert(response.end(),
                    buffer + (12 + (is_tcp ? 2 : 0)),
                    buffer + question_end);

    // ---------------- ANSWER ----------------
    if (qclass == 1 && qdcount == 1)
    {
        for (auto &record : matched_records)
        {
            if (!cacheHit)
                Operational::addQueueForRecord(nameWire, qtype, record);

            if (rateLimiting && !is_tcp)
            {
                RRLKey key;
                key.prefix = ip;
                key.rcode = 0;

                uint32_t zone;
                memcpy(&zone, zoneData.data(), sizeof(uint32_t));

                key.zone_id = zone;

                uint32_t current = now();

                auto &bucket = thread.rrlBuckets[key];

                if (bucket.window_start != current)
                {
                    bucket.window_start = current;
                    bucket.responses = 1;
                }
                else
                {
                    bucket.responses += 1;
                }

                if (bucket.responses > threshold)
                {
                    truncated = true;
                }
            }



            if (record.is_geo)
            {
                std::vector<IpGroupEntry::IpGroupEntryResponse> out_entries;
                Main::ipGroupMap->get_record(record.group_id, "AF", out_entries);

                if (out_entries.size() == 0)
                {
                    out_entries = DB::Record::getIpGroupEntriesCountryBased(record.group_id, "AF");

                    for (auto &entry : out_entries)
                    {
                        Operational::addQueueForEntry(record.group_id, "AF", {entry.ip, entry.priority});
                    }

                    record.rdata = out_entries[0].ip;
                }
                else
                {
                    record.rdata = out_entries[0].ip;
                }
            }

            uint16_t udp_limit = edns_class ? edns_class : 512;
            size_t rr_size =
                2 + // pointer
                2 + // type
                2 + // class
                4 + // ttl
                2 + // rdlen
                record.rdata.size();

            if (!is_tcp && response.size() + rr_size > udp_limit)
            {
                truncated = true;
            }

            if (!truncated)
            {
                write16(response, 0xC00C); // pointer
                write16(response, qtype);
                write16(response, 1); // IN (qclass)
                write32(response, record.ttl);

                if (qtype == 15)
                {
                    write16(response, static_cast<uint16_t>(2 + record.rdata.size()));
                    write16(response, record.priority);
                    response.insert(response.end(), record.rdata.begin(), record.rdata.end());
                }
                else if (qtype == 16)
                {
                    write16(response, static_cast<uint16_t>(record.rdata.size()));

                    response.insert(response.end(), record.rdata.begin(), record.rdata.end());
                }
                else
                {
                    write16(response, record.rdata.size());

                    response.insert(response.end(),
                                    record.rdata.begin(),
                                    record.rdata.end());
                }

                anc++;
            }
        }

        if (anc == 0)
        {
            // NOERROR, empty answer
        }
    }

    if (truncated)
    {
        response_flags |= 0x0200; // TC (TCP) Truncated
    }

    // ---------------- FIX HEADER ----------------
    response[6] = (anc >> 8) & 0xFF;
    response[7] = anc & 0xFF;

    response[2] = (response_flags >> 8) & 0xFF;
    response[3] = response_flags & 0xFF;

    response[10] = 0;
    response[11] = 1;

    response.push_back(0);   // Name: . (Root)
    write16(response, 41);   // Type: OPT (EDNS0)
    write16(response, 4096); // Payload size: 4096
    write32(response, 0);    // TTL: 0
    write16(response, 0);    // RDLEN: 0

    if (is_tcp)
    {
        std::vector<uint8_t> res;
        write16(res, response.size());
        res.insert(res.end(), response.begin(), response.end());
        return res;
    }
    else
        return response;
}

int Core::makeNonBlocking(int sfd)
{
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    flags |= O_NONBLOCK;
    return fcntl(sfd, F_SETFL, flags);
}

void Core::write32(std::vector<uint8_t> &buf, uint32_t value)
{
    uint32_t net = htonl(value);
    uint8_t *p = (uint8_t *)&net;
    buf.push_back(p[0]);
    buf.push_back(p[1]);
    buf.push_back(p[2]);
    buf.push_back(p[3]);
}

void Core::write16(std::vector<uint8_t> &buf, uint16_t value)
{
    uint16_t net = htons(value);
    uint8_t *p = (uint8_t *)&net;
    buf.push_back(p[0]);
    buf.push_back(p[1]);
}

uint32_t Core::now()
{
    return g_second.load(std::memory_order_relaxed);
}

void Core::start_clock_thread()
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