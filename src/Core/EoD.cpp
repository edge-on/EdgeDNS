#include "Core/EoD.hpp"

EoD::EoD() : activeThreads({})
{
}

EoD::~EoD()
{
}

void EoD::start()
{
    threadCount = std::thread::hardware_concurrency();

    start_clock_thread();

    for (int i = 0; i < threadCount; ++i)
    {
        threads.emplace_back(&EoD::worker, this, i);
    }

    std::thread ipc_thread = std::thread(&EoD::initIPC, this);

    for (auto &t : threads)
    {
        t.join();
    }

    ipc_thread.join();
}

void EoD::worker(int th)
{
    Thread &thread = activeThreads[th];

    thread.epoll_fd = epoll_create1(0);

    initUDP(thread);
    initTCP(thread);

    epoll_event events[max_event];

    while (true)
    {
        int n = epoll_wait(thread.epoll_fd, events, max_event, -1);

        for (int i = 0; i < n; ++i)
        {
            if (events[i].data.fd == thread.eod_udp_fd)
            {
                handleUDP(thread);
            }
            else if (events[i].data.fd == thread.eod_tcp_fd)
            {
                while (true)
                {
                    sockaddr_in event{};
                    socklen_t len = sizeof(event);

                    int client_fd = accept(thread.eod_tcp_fd, (sockaddr *)&event, &len);

                    if (client_fd == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            break;
                        }
                        else
                        {
                            perror("accept");
                            break;
                        }
                    }

                    makeNonBlocking(client_fd);

                    Connection conn{};
                    conn.fd = client_fd;
                    conn.ip = htons(event.sin_addr.s_addr);

                    thread.connections.emplace(client_fd, std::move(conn));

                    epoll_event e{};
                    e.data.fd = client_fd;
                    e.events = EPOLLIN | EPOLLET;

                    epoll_ctl(thread.epoll_fd, EPOLL_CTL_ADD, client_fd, &e);
                }
            }
            else
            {
                auto it = thread.connections.find(events[i].data.fd);
                if (it == thread.connections.end())
                {
                    continue;
                }

                Connection &conn = it->second;

                if (events[i].events & EPOLLIN)
                {
                    handleTCP(conn, thread);
                }

                if (events[i].events & EPOLLOUT)
                {
                    writeTCP(conn, thread);
                }
            }
        }
    }
}

void EoD::initUDP(Thread &thread)
{
    thread.eod_udp_fd = socket(AF_INET, SOCK_DGRAM, 0);

    makeNonBlocking(thread.eod_udp_fd);

    if (thread.eod_udp_fd == 0)
    {
        perror("eod socket");
    }

    int opt = 1;
    setsockopt(thread.eod_udp_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in eod_addr{};
    eod_addr.sin_family = AF_INET;
    eod_addr.sin_addr.s_addr = INADDR_ANY;
    eod_addr.sin_port = htons(eod_port);

    if (bind(thread.eod_udp_fd, (sockaddr *)&eod_addr, sizeof(eod_addr)) < 0)
    {
        perror("eod bind");
    }

    epoll_event event{};
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = thread.eod_udp_fd;

    if (epoll_ctl(thread.epoll_fd, EPOLL_CTL_ADD, thread.eod_udp_fd, &event) < 0)
    {
        perror("epoll ctl");
    }
}

void EoD::handleUDP(Thread &thread)
{
    constexpr int BATCH = 256;
    constexpr int BUF_SIZE = 4096;

    struct mmsghdr recv_msgs[BATCH];
    struct iovec recv_iovecs[BATCH];
    struct sockaddr_in clients[BATCH];
    uint8_t recv_buffers[BATCH][BUF_SIZE];

    struct mmsghdr send_msgs[BATCH];
    struct iovec send_iovecs[BATCH];
    uint8_t send_buffers[BATCH][BUF_SIZE];

    for (int i = 0; i < BATCH; i++)
    {
        recv_iovecs[i].iov_base = recv_buffers[i];
        recv_iovecs[i].iov_len = BUF_SIZE;

        recv_msgs[i].msg_hdr.msg_iov = &recv_iovecs[i];
        recv_msgs[i].msg_hdr.msg_iovlen = 1;
        recv_msgs[i].msg_hdr.msg_name = &clients[i];
        recv_msgs[i].msg_hdr.msg_namelen = sizeof(sockaddr_in);
        recv_msgs[i].msg_hdr.msg_control = nullptr;
        recv_msgs[i].msg_hdr.msg_controllen = 0;
        recv_msgs[i].msg_hdr.msg_flags = 0;
    }

    int received = recvmmsg(
        thread.eod_udp_fd,
        recv_msgs,
        BATCH,
        MSG_WAITFORONE,
        nullptr);

    if (received <= 0)
        return;

    for (int i = 0; i < received; i++)
    {
        int len = recv_msgs[i].msg_len;

        uint32_t ip = ntohl(clients[i].sin_addr.s_addr);

        std::vector<uint8_t> resp =
            handle(recv_buffers[i], false, ip, thread);

        memcpy(send_buffers[i], resp.data(), resp.size());

        send_iovecs[i].iov_base = send_buffers[i];
        send_iovecs[i].iov_len = resp.size();

        send_msgs[i].msg_hdr.msg_iov = &send_iovecs[i];
        send_msgs[i].msg_hdr.msg_iovlen = 1;
        send_msgs[i].msg_hdr.msg_name = &clients[i];
        send_msgs[i].msg_hdr.msg_namelen = sizeof(sockaddr_in);
        send_msgs[i].msg_hdr.msg_control = nullptr;
        send_msgs[i].msg_hdr.msg_controllen = 0;
        send_msgs[i].msg_hdr.msg_flags = 0;
    }

    int sent = sendmmsg(
        thread.eod_udp_fd,
        send_msgs,
        received,
        0);

    (void)sent;
}

void EoD::initTCP(Thread &thread)
{
    thread.eod_tcp_fd = socket(AF_INET, SOCK_STREAM, 0);

    makeNonBlocking(thread.eod_tcp_fd);

    if (thread.eod_tcp_fd == 0)
    {
        perror("eod tcp socket");
    }

    int opt = 1;
    setsockopt(thread.eod_tcp_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in eod_addr{};
    eod_addr.sin_family = AF_INET;
    eod_addr.sin_addr.s_addr = INADDR_ANY;
    eod_addr.sin_port = htons(eod_port);

    if (bind(thread.eod_tcp_fd, (sockaddr *)&eod_addr, sizeof(eod_addr)) < 0)
    {
        perror("eod tcp bind");
    }

    if (listen(thread.eod_tcp_fd, SOMAXCONN) < 0)
    {
        perror("eod tcp listen");
    }

    epoll_event event{};
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = thread.eod_tcp_fd;

    if (epoll_ctl(thread.epoll_fd, EPOLL_CTL_ADD, thread.eod_tcp_fd, &event) < 0)
    {
        perror("epoll tcp ctl");
    }
}

void EoD::handleTCP(Connection &conn, Thread &thread)
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

        auto response = handle(dnsPacket.data(), true, conn.ip, thread);

        uint16_t len = htons(response.size());

        conn.writeBuffer.insert(conn.writeBuffer.end(),
                                (uint8_t *)&len,
                                (uint8_t *)&len + 2);

        conn.writeBuffer.insert(conn.writeBuffer.end(),
                                response.begin(),
                                response.end());

        enableWrite(conn.fd, thread.epoll_fd);
    }
}

void EoD::writeTCP(Connection &conn, Thread &thread)
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

    disableWrite(conn.fd, thread.epoll_fd);
}

void EoD::initIPC()
{
    eod_ipc_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (eod_ipc_fd < 0)
    {
        perror("eod_ipc_fd socket");
    }

    makeNonBlocking(eod_ipc_fd);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/run/eod.sock");

    unlink("/run/eod.sock");

    if (bind(eod_ipc_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("eod_ipc_fd bind");
    }

    if (listen(eod_ipc_fd, SOMAXCONN) < 0)
    {
        perror("eod_ipc_fd listen");
    }

    while (true)
    {
        int client_fd = accept(eod_ipc_fd, nullptr, nullptr);
        if (client_fd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            perror("accept");
            continue;
        }

        handleIPC(client_fd);
        close(client_fd);
    }
}

void EoD::handleIPC(int fd)
{
    std::vector<uint8_t> buffer(1024);

    read(fd, buffer.data(), buffer.size());

    int offset = 0;
    uint8_t code = buffer[offset];
    offset++;

    if (code == IPC::Commands::RELOAD)
    {
        std::vector<uint8_t> response;
        response.push_back(IPC::Commands::DONE);

        std::string zone = Utils::Vector::wireToDomain(buffer.data() + offset, buffer.size() - offset);
        DNS::reloadZone(zone);

        if (is_logging)
        {
            std::cout << "Zone " << zone << " Reloaded!" << std::endl;
        }

        send(fd, response.data(), response.size(), 0);
    }
}

std::vector<uint8_t> EoD::handle(uint8_t buffer[4096], bool is_tcp, uint32_t ip, Thread &thread)
{
    if (is_logging)
    {
        std::cout << (is_tcp ? "TCP " : "UDP ") << "Request" << std::endl;
    }

    bool truncated = false;

    size_t offset = 0;

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

    // ---------------- FIND ZONE (Longest suffix) ----------------
    std::vector<uint8_t> zoneWire;

    size_t i = 0;
    while (i < nameWire.size() && nameWire[i] != 0)
    {
        std::vector<uint8_t> candidate(nameWire.begin() + i,
                                       nameWire.end());

        if (zones.find(candidate) != zones.end())
        {
            zoneWire = candidate;
            break;
        }

        i += nameWire[i] + 1;
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

    std::vector<uint8_t> response;

    write16(response, transaction_id);

    uint16_t response_flags = 0;
    response_flags |= 0x8000;           // QR
    response_flags |= (flags & 0x0100); // RD mirror
    response_flags |= 0x0400;           // AA

    uint16_t anc = 0;

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
        response_flags |= 0x0005; // NOTIMP
    }

    write16(response, response_flags);
    write16(response, 1); // QDCOUNT
    write16(response, 0); // ANCOUNT placeholder
    write16(response, 0); // NSCOUNT
    write16(response, 0); // ARCOUNT

    // copy question
    response.insert(response.end(),
                    buffer + (12),
                    buffer + question_end);

    // ---------------- ANSWER ----------------
    if (qclass == 1 && qdcount == 1)
    {
        if (!zoneWire.empty())
        {
            auto zoneIt = zones.find(zoneWire);
            auto nameIt = zoneIt->second->names.find(nameWire);

            if (nameIt != zoneIt->second->names.end())
            {
                auto &list = nameIt->second;

                for (size_t i = 0; i < list.size();)
                {
                    auto r_it = records.find(list[i]);

                    if (r_it == records.end())
                    {
                        list[i] = list.back();
                        list.pop_back();
                        continue;
                    }

                    Record &rec = r_it->second;
                    UUIDKey record = list[i];
                    
                    if (records[record].type != qtype)
                        continue;

                    if (is_rrl)
                    {
                        RRLKey key;
                        key.prefix = ip;
                        key.rcode = 0;

                        uint32_t zone;
                        memcpy(&zone, zoneWire.data(), sizeof(uint32_t));

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

                    if (!truncated)
                    {
                        write16(response, 0xC00C); // pointer
                        write16(response, records[record].type);
                        write16(response, 1); // IN (qclass)
                        write32(response, records[record].ttl);
                        write16(response, records[record].rdata.size());

                        response.insert(response.end(),
                                        records[record].rdata.begin(),
                                        records[record].rdata.end());

                        anc++;
                    }

                    i++;
                }

                if (anc == 0)
                {
                    // NOERROR, empty answer
                }
            }
            else
            {
                if (!truncated)
                {
                    response_flags |= 0x0003; // NXDOMAIN
                }
            }
        }
        else
        {
            if (!truncated)
            {
                response_flags |= 0x0005; // REFUSED
            }
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

    return response;
}

void EoD::enableWrite(int fd, int epoll_fd)
{
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = fd;

    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

void EoD::disableWrite(int fd, int epoll_fd)
{
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;

    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

int EoD::makeNonBlocking(int sfd)
{
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    flags |= O_NONBLOCK;
    return fcntl(sfd, F_SETFL, flags);
}

void EoD::write32(std::vector<uint8_t> &buf, uint32_t value)
{
    uint32_t net = htonl(value);
    uint8_t *p = (uint8_t *)&net;
    buf.push_back(p[0]);
    buf.push_back(p[1]);
    buf.push_back(p[2]);
    buf.push_back(p[3]);
}

void EoD::write16(std::vector<uint8_t> &buf, uint16_t value)
{
    uint16_t net = htons(value);
    uint8_t *p = (uint8_t *)&net;
    buf.push_back(p[0]);
    buf.push_back(p[1]);
}

uint32_t EoD::now()
{
    return g_second.load(std::memory_order_relaxed);
}

void EoD::start_clock_thread()
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