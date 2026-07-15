#include "Core/Core.hpp"

Core::Core()
{
}

Core::~Core()
{
}

void Core::start()
{
    threadCount = 12;

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

    Utils::Socket::makeNonBlocking(thread.udpFd);

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

    Utils::Socket::makeNonBlocking(thread.tcpFd);

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
        perror("eod tcp bind");

    if (listen(thread.tcpFd, SOMAXCONN) < 0)
        perror("eod tcp listen");

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
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));
    params.flags = IORING_SETUP_SQPOLL;
    params.sq_thread_idle = 2000;
    if (io_uring_queue_init_params(QUEUE_DEPTH, ring, &params) < 0)
    {
        perror("io uring queue init with SQPOLL");
        return;
    }

    Pipeline *pipeline = new Pipeline();
    pipeline->init(th);

    initUDP(thread);
    initTCP(thread);

    Sync::initSync(2987, thread);

    pipeline->queueMultishotAccept(thread.tcpFd);
    pipeline->queueMultishotAccept(thread.syncFd);

    pipeline->queueRead(thread.connections[thread.udpFd]);

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
            auto it = thread.connections.find(fd);
            if (it != thread.connections.end())
            {
                Gen::Connection &errConn = it->second;

                if (errConn.type == Gen::UDP && fd == thread.udpFd)
                {
                    std::cerr << "UDP recv error, re-arming: " << strerror(-res) << std::endl;
                    pipeline->queueRead(errConn);
                    io_uring_submit(ring);
                }
                else
                {
                    thread.connections.erase(fd);
                    close(fd);
                }
            }

            if (type == Gen::STATE_MULTISHOT_ACCEPT)
            {
                pipeline->queueMultishotAccept(fd);
                io_uring_submit(ring);
            }

            continue;
        }

        if (type == Gen::STATE_MULTISHOT_ACCEPT)
        {
            int t = fd == thread.tcpFd ? Gen::TCP : Gen::SYNC;
            int clientFd = res;

            Gen::Connection conn;
            conn.fd = clientFd;
            conn.type = t;

            struct sockaddr_storage addr;
            socklen_t len = sizeof(addr);

            if (getpeername(clientFd, (struct sockaddr *)&addr, &len) == 0)
            {
                char ip_str[INET6_ADDRSTRLEN] = {0};

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

            if (conn.type == Gen::SYNC && strcmp(conn.ip_str, "127.0.0.1") != 0)
            {
                close(res);
                continue;
            }

            thread.connections[clientFd] = std::move(conn);

            if (conn.type == Gen::SYNC)
                pipeline->queueReadForSync(thread.connections[clientFd]);
            else
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
            int group_id = conn.buf_group;
            char *raw = pipeline->pool->getBufferAddress(group_id, buf_id);
            size_t buf_size = pipeline->pool->getBufferSize();

            if (res < 0)
            {
                std::cerr << "recvmsg failed: " << strerror(-cqe->res)
                          << " - FD: " << conn.fd << " Res: " << cqe->res << std::endl;
                pipeline->pool->releaseBuffer(group_id, buf_id);
                break;
            }

            struct io_uring_recvmsg_out *o = io_uring_recvmsg_validate(raw, cqe->res, &conn.msgHdr);
            if (!o)
            {
                pipeline->pool->releaseBuffer(group_id, buf_id);
                break;
            }

            void *payload = io_uring_recvmsg_payload(o, &conn.msgHdr);
            size_t payload_len = io_uring_recvmsg_payload_length(o, cqe->res, &conn.msgHdr);

            uint8_t queryBuf[4096];
            size_t queryLen = std::min(payload_len, sizeof(queryBuf));
            memcpy(queryBuf, payload, queryLen);

            pipeline->pool->releaseBuffer(group_id, buf_id);

            switch (conn.type)
            {
            case Gen::SYNC:
            {
                char bufType[32] = {0};

                // ==============================
                // Types
                // ==============================
                // 0-1 - Record Add
                // 0-2 - Reocrd Update
                // 0-3 - Record Delete
                // ==============================
                // 1-1 - Ip Group Entry Add
                // 1-2 - Ip Group Entry Updated
                // 1-3 - Ip Group Entry Delete
                // ==============================

                int result;

                char *response = "HTTP/1.1 400 Bad Request\r\n"
                                 "Content-Type: text/plain\r\n"
                                 "Connection: close\r\n"
                                 "Content-Length: 11\r\n"
                                 "\r\n"
                                 "Bad Request";

                if (Utils::String::getParamFromCharBuffer((char *)queryBuf, "type", bufType, sizeof(bufType)))
                {
                    char op = bufType[2];

                    // ==============================
                    // Operations
                    // ==============================
                    // 1 = Add              =========
                    // 2 = Update           =========
                    // 3 = Delete           =========
                    // ==============================

                    // ==============================
                    // Responses
                    // ==============================
                    // 0 = Success          =========
                    // 1 = Not Found        =========
                    // 2 = Error            =========
                    // ==============================

                    if (op != '1' && op != '2' && op != '3')
                    {
                        // If there are an valid op, continue, if there are not stop and return bad request!
                    }
                    else if (bufType[0] == '0' && bufType[1] == '-') // Record Operations
                    {
                        char zone[32] = {0};
                        char type[32] = {0};
                        char name[32] = {0};
                        char idStr[32] = {0};

                        if (Utils::String::getParamFromCharBuffer((char *)queryBuf, "zone", zone, sizeof(zone)) &&
                            Utils::String::getParamFromCharBuffer((char *)queryBuf, "type", type, sizeof(type)) &&
                            Utils::String::getParamFromCharBuffer((char *)queryBuf, "name", name, sizeof(name)) &&
                            Utils::String::getParamFromCharBuffer((char *)queryBuf, "idStr", idStr, sizeof(idStr)))
                        {
                            CassUuid id;
                            cass_uuid_from_string(idStr, &id);
                        }
                    }
                    else if (bufType[0] == '1' && bufType[1] == '-') // Ip Group Entry Operations
                    {
                        char groupIdStr[40] = {0};
                        char idStr[40] = {0};
                        char locationCode[32] = {0};
                        char ip[16] = {0};
                        char priority[32] = {0};

                        if (Utils::String::getParamFromCharBuffer((char *)queryBuf, "group_id", groupIdStr, sizeof(groupIdStr)) &&
                            Utils::String::getParamFromCharBuffer((char *)queryBuf, "db_id", idStr, sizeof(idStr)) &&
                            Utils::String::getParamFromCharBuffer((char *)queryBuf, "location_code", locationCode, sizeof(locationCode)) &&
                            Utils::String::getParamFromCharBuffer((char *)queryBuf, "ip_address", ip, sizeof(ip)) &&
                            Utils::String::getParamFromCharBuffer((char *)queryBuf, "priority", priority, sizeof(priority)))
                        {
                            CassUuid id;
                            cass_uuid_from_string(idStr, &id);

                            CassUuid groupId;
                            cass_uuid_from_string(groupIdStr, &groupId);

                            std::vector<IpGroupEntry::IpGroupEntryResponse> out;
                            Main::ipGroupMap->get_record(groupId, locationCode, out);

                            if (out.size() == 0 && (op == '3'))
                                response = "HTTP/1.1 200 OK\r\n"
                                           "Content-Type: text/plain\r\n"
                                           "Connection: close\r\n"
                                           "Content-Length: 1\r\n"
                                           "\r\n"
                                           "1";
                            else
                                switch (op)
                                {
                                case '1': // Add
                                {
                                    Operational::addQueueForEntry(groupId, locationCode, {id, RData::generateRData(ip, 1), atoi(priority)});
                                    response = "HTTP/1.1 200 OK\r\n"
                                               "Content-Type: text/plain\r\n"
                                               "Connection: close\r\n"
                                               "Content-Length: 1\r\n"
                                               "\r\n"
                                               "0";
                                    break;
                                }

                                case '2': // Update
                                {
                                    Main::ipGroupMap->delete_record(groupId, locationCode, atoi(priority));

                                    response = "HTTP/1.1 200 OK\r\n"
                                               "Content-Type: text/plain\r\n"
                                               "Connection: close\r\n"
                                               "Content-Length: 1\r\n"
                                               "\r\n"
                                               "0";
                                    break;
                                }

                                case '3': // Delete
                                {
                                    response = "HTTP/1.1 200 OK\r\n"
                                               "Content-Type: text/plain\r\n"
                                               "Connection: close\r\n"
                                               "Content-Length: 1\r\n"
                                               "\r\n"
                                               "0";
                                    break;
                                }
                                }
                        }
                    }
                }

                conn.len = strlen(response);
                memcpy(conn.writeBuffer, response, conn.len);

                pipeline->queueWriteTcp(conn);
                io_uring_submit(ring);
                break;
            }

            case Gen::TCP:
            {
                conn.len = handle(queryBuf, true, conn.ip, conn.ip_str, thread, conn.writeBuffer);
                pipeline->queueWriteTcp(conn);
                io_uring_submit(ring);
                break;
            }

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

                ctx->len = handle(queryBuf, false, conn.ip, conn.ip_str, thread, ctx->writeBuffer);

                ctx->iov.iov_base = ctx->writeBuffer;
                ctx->iov.iov_len = ctx->len;

                pipeline->queueWriteUdp(ctx);
                io_uring_submit(ring);
                break;
            }
            }

            if (!hasMore)
            {
                if (conn.type == Gen::SYNC)
                    pipeline->queueReadForSync(conn);
                else
                    pipeline->queueRead(conn);

                io_uring_submit(ring);
            }

            break;
        }

        case Gen::STATE_WRITE:
        {
            if (conn.type == Gen::SYNC)
            {
                pipeline->queueReadForSync(conn);
                io_uring_submit(ring);
            }
            break;
        }
        }
    }
}

ssize_t Core::handle(uint8_t *buffer, bool is_tcp, uint32_t ip, char *ip_str, Gen::Thread &thread, uint8_t (&out)[4096])
{
    if (isLogging)
    {
        std::cout << (is_tcp ? "TCP " : "UDP ") << "Request" << std::endl;
    }

    bool truncated = false;
    size_t offset = is_tcp ? 2 : 0;

    auto read16 = [&]() -> uint16_t
    {
        uint16_t v = (buffer[offset] << 8) | buffer[offset + 1];
        offset += 2;
        return v;
    };

    uint16_t transaction_id = read16();
    uint16_t flags = read16();
    uint16_t qdcount = read16();
    read16(); // ancount
    read16(); // nscount
    read16(); // arcount

    uint8_t nameWire[256];
    size_t name_start = offset;

    while (buffer[offset] != 0)
    {
        uint8_t len = buffer[offset++];
        offset += len;
    }
    offset++;

    size_t nameLen = offset - name_start;
    if (nameLen > sizeof(nameWire))
        nameLen = sizeof(nameWire);

    memcpy(nameWire, buffer + name_start, nameLen);

    uint16_t qtype = read16();
    uint16_t qclass = read16();
    size_t question_end = offset;

    offset++; // NAME
    uint16_t edns_type = read16();
    uint16_t edns_class = read16();
    read16(); // TTL hi
    read16(); // TTL lo
    read16(); // RDLEN

    for (size_t i = 0; i < nameLen; i++)
    {
        if (nameWire[i] >= 'A' && nameWire[i] <= 'Z')
            nameWire[i] += 32;
    }

    size_t rlen = 0;

    auto w16 = [&](uint16_t v)
    {
        out[rlen++] = (v >> 8) & 0xFF;
        out[rlen++] = v & 0xFF;
    };
    auto w32 = [&](uint32_t v)
    {
        out[rlen++] = (v >> 24) & 0xFF;
        out[rlen++] = (v >> 16) & 0xFF;
        out[rlen++] = (v >> 8) & 0xFF;
        out[rlen++] = v & 0xFF;
    };

    uint16_t response_flags = 0;
    uint16_t anc = 0;

    t_matchedRecords.clear();

    bool cacheHit = Main::recordsMap->get_record(nameWire, nameLen, qtype, t_matchedRecords);

    uint8_t zoneBuf[256];
    size_t zoneLen = 0;

    if (!cacheHit || rateLimiting)
    {
        zoneLen = Utils::Vector::getHostWireFromWireNoAlloc(
            nameWire, nameLen, zoneBuf, sizeof(zoneBuf));
    }

    if (!cacheHit)
    {
        std::string name = Utils::Vector::wireToDomain(nameWire, nameLen);
        std::string zone = Utils::Vector::wireToDomain(zoneBuf, zoneLen);
        t_matchedRecords = DB::Record::getRecord(zone, name, qtype);
    }

    // ---------------- BUILD RESPONSE ----------------
    w16(transaction_id);

    response_flags |= 0x8000;           // QR
    response_flags |= (flags & 0x0100); // RD mirror
    response_flags |= 0x0400;           // AA

    if (qclass != 1)
        response_flags |= 0x0004; // NOTIMP
    if (qdcount != 1)
        response_flags |= 0x0001; // FORMERR
    if (qtype != 1)
        response_flags |= 0x0005; // REFUSED

    w16(response_flags);
    w16(1); // QDCOUNT
    w16(0); // ANCOUNT placeholder
    w16(0); // NSCOUNT
    w16(0); // ARCOUNT

    size_t qstart = 12 + (is_tcp ? 2 : 0);
    size_t qbytes = question_end - qstart;
    memcpy(out + rlen, buffer + qstart, qbytes);
    rlen += qbytes;

    // ---------------- ANSWER ----------------
    if (qclass == 1 && qdcount == 1)
    {
        for (auto &record : t_matchedRecords)
        {
            if (!cacheHit)
                Operational::addQueueForRecord(nameWire, nameLen, qtype, record);

            if (rateLimiting && !is_tcp)
            {
                RRLKey key;
                key.prefix = ip;
                key.rcode = 0;

                uint32_t zone;
                memcpy(&zone, zoneBuf, sizeof(uint32_t));
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
                    truncated = true;
            }

            if (record.is_geo)
            {
                t_ipGroupEntries.clear();
                Main::ipGroupMap->get_record(record.group_id, "AF", t_ipGroupEntries);

                if (t_ipGroupEntries.empty())
                {
                    t_ipGroupEntries = DB::Record::getIpGroupEntriesCountryBased(record.group_id, "AF");

                    for (auto &entry : t_ipGroupEntries)
                        Operational::addQueueForEntry(record.group_id, "AF", {entry.id, entry.ip, entry.priority});
                }

                for (auto &entry : t_ipGroupEntries)
                {
                    record.rdata = entry.ip;

                    uint16_t udp_limit = edns_class ? edns_class : 512;
                    size_t rr_size =
                        2 + // pointer
                        2 + // type
                        2 + // class
                        4 + // ttl
                        2 + // rdlen
                        record.rdata.size();

                    if (!is_tcp && rlen + rr_size > udp_limit)
                        truncated = true;

                    if (rlen + rr_size + 32 > sizeof(out))
                        truncated = true;

                    if (!truncated)
                    {
                        w16(0xC00C); // pointer
                        w16(qtype);
                        w16(1); // IN
                        w32(record.ttl);

                        if (qtype == 15) // MX
                        {
                            w16(static_cast<uint16_t>(2 + record.rdata.size()));
                            w16(record.priority);
                            memcpy(out + rlen, record.rdata.data(), record.rdata.size());
                            rlen += record.rdata.size();
                        }
                        else
                        {
                            w16(static_cast<uint16_t>(record.rdata.size()));
                            memcpy(out + rlen, record.rdata.data(), record.rdata.size());
                            rlen += record.rdata.size();
                        }

                        anc++;
                    }
                }

                continue;
            }

            uint16_t udp_limit = edns_class ? edns_class : 512;
            size_t rr_size =
                2 + // pointer
                2 + // type
                2 + // class
                4 + // ttl
                2 + // rdlen
                record.rdata.size();

            if (!is_tcp && rlen + rr_size > udp_limit)
                truncated = true;

            if (rlen + rr_size + 32 > sizeof(out))
                truncated = true;

            if (!truncated)
            {
                w16(0xC00C); // pointer
                w16(qtype);
                w16(1); // IN
                w32(record.ttl);

                if (qtype == 15) // MX
                {
                    w16(static_cast<uint16_t>(2 + record.rdata.size()));
                    w16(record.priority);
                    memcpy(out + rlen, record.rdata.data(), record.rdata.size());
                    rlen += record.rdata.size();
                }
                else
                {
                    w16(static_cast<uint16_t>(record.rdata.size()));
                    memcpy(out + rlen, record.rdata.data(), record.rdata.size());
                    rlen += record.rdata.size();
                }

                anc++;
            }
        }
    }

    if (truncated)
        response_flags |= 0x0200; // TC

    // ---------------- FIX HEADER ----------------
    out[6] = (anc >> 8) & 0xFF;
    out[7] = anc & 0xFF;
    out[2] = (response_flags >> 8) & 0xFF;
    out[3] = response_flags & 0xFF;
    out[10] = 0;
    out[11] = 1;

    // OPT / EDNS0
    if (rlen + 11 > sizeof(out))
        rlen = sizeof(out) - 11;

    out[rlen++] = 0; // Name: . (Root)
    w16(41);         // Type: OPT
    w16(4096);       // Payload size
    w32(0);          // TTL
    w16(0);          // RDLEN

    if (is_tcp)
    {
        out[0] = (rlen >> 8) & 0xFF;
        out[1] = rlen & 0xFF;
        memcpy(out + 2, out, rlen);
    }

    return rlen;
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