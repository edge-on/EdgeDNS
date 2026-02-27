#include "Core/EoD.hpp"

EoD::EoD()
{
}

EoD::~EoD()
{
}

void EoD::start()
{
    epoll_fd = epoll_create1(0);

    initUDP();
    initTCP();

    epoll_event events[max_event];

    while (true)
    {
        int n = epoll_wait(epoll_fd, events, max_event, -1);

        for (int i = 0; i < n; ++i)
        {
            if (events[i].data.fd == eod_udp_fd)
            {
                handleUDP();
            }
            else if (events[i].data.fd == eod_tcp_fd)
            {
                sockaddr_in event{};

                while (true)
                {
                    socklen_t len = sizeof(event);

                    int client_fd = accept(eod_tcp_fd, (sockaddr *)&event, &len);

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

                    connections.emplace(client_fd, std::move(conn));

                    epoll_event e{};
                    e.data.fd = client_fd;
                    e.events = EPOLLIN | EPOLLET;

                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &e);
                }
            }
            else
            {
                auto it = connections.find(events[i].data.fd);
                if (it == connections.end())
                {
                    continue;
                }

                Connection &conn = it->second;

                if (events[i].events & EPOLLIN)
                {
                    handleTCP(conn);
                }

                if (events[i].events & EPOLLOUT)
                {
                    writeTCP(conn);
                }
            }
        }
    }
}

void EoD::initUDP()
{
    eod_udp_fd = socket(AF_INET, SOCK_DGRAM, 0);

    makeNonBlocking(eod_udp_fd);

    if (eod_udp_fd == 0)
    {
        perror("eod socket");
    }

    sockaddr_in eod_addr{};
    eod_addr.sin_family = AF_INET;
    eod_addr.sin_addr.s_addr = INADDR_ANY;
    eod_addr.sin_port = htons(eod_port);

    if (bind(eod_udp_fd, (sockaddr *)&eod_addr, sizeof(eod_addr)) < 0)
    {
        perror("eod bind");
    }

    epoll_event event{};
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = eod_udp_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, eod_udp_fd, &event) < 0)
    {
        perror("epoll ctl");
    }
}

void EoD::handleUDP()
{
    uint8_t buffer[4096];

    sockaddr_in client{};
    socklen_t len = sizeof(client);

    size_t received = recvfrom(eod_udp_fd, buffer, sizeof(buffer), 0, (sockaddr *)&client, &len);

    std::vector<uint8_t> response = handle(buffer, false);

    sendto(eod_udp_fd, response.data(), response.size(), 0,
           (sockaddr *)&client, len);
}

void EoD::initTCP()
{
    eod_tcp_fd = socket(AF_INET, SOCK_STREAM, 0);

    makeNonBlocking(eod_tcp_fd);

    if (eod_tcp_fd == 0)
    {
        perror("eod tcp socket");
    }

    sockaddr_in eod_addr{};
    eod_addr.sin_family = AF_INET;
    eod_addr.sin_addr.s_addr = INADDR_ANY;
    eod_addr.sin_port = htons(eod_port);

    if (bind(eod_tcp_fd, (sockaddr *)&eod_addr, sizeof(eod_addr)) < 0)
    {
        perror("eod tcp bind");
    }

    if (listen(eod_tcp_fd, SOMAXCONN) < 0)
    {
        perror("eod tcp listen");
    }

    epoll_event event{};
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = eod_tcp_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, eod_tcp_fd, &event) < 0)
    {
        perror("epoll tcp ctl");
    }
}

void EoD::handleTCP(Connection &conn)
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

        auto response = handle(dnsPacket.data(), false);

        uint16_t len = htons(response.size());

        conn.writeBuffer.insert(conn.writeBuffer.end(),
                                (uint8_t *)&len,
                                (uint8_t *)&len + 2);

        conn.writeBuffer.insert(conn.writeBuffer.end(),
                                response.begin(),
                                response.end());

        enableWrite(conn.fd, epoll_fd);
    }
}

void EoD::writeTCP(Connection &conn)
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

    disableWrite(conn.fd, epoll_fd);
}

std::vector<uint8_t> EoD::handle(uint8_t buffer[4096], bool is_tcp)
{
    size_t offset = is_tcp ? 2 : 0;

    auto read16 = [&](void)
    {
        uint16_t value = (buffer[offset] << 8) | buffer[offset + 1];
        offset += 2;
        return value;
    };

    uint16_t transcation_id = read16(); // Transcation ID
    uint16_t flags = read16();          // Flags
    uint16_t qdcount = read16();
    uint16_t ancount = read16();
    uint16_t nscount = read16();
    uint16_t arcount = read16();

    std::string name;

    while (buffer[offset] != 0)
    {
        size_t len = buffer[offset++];

        for (int i = 0; i < len; ++i)
        {
            name += (char)buffer[offset++];
        }
        name += ".";
    }

    offset++;

    uint16_t qtype = read16();
    uint16_t qclass = read16();

    size_t question_end = offset;

    std::cout << "=== HEADER ===" << std::endl;
    std::cout << "Transcation ID: " << transcation_id << std::endl;
    std::cout << "Flags: " << flags << std::endl;
    std::cout << "QD Count: " << qdcount << std::endl;
    std::cout << "AN Count: " << ancount << std::endl;
    std::cout << "NS Count: " << nscount << std::endl;
    std::cout << "AR Count: " << arcount << std::endl;

    std::cout << "=== QUESTIONS ===" << std::endl;
    std::cout << "Q Name: " << name << std::endl;
    std::cout << "Q Type: " << qtype << std::endl;
    std::cout << "Q Class: " << qclass << std::endl;

    // Additional Section
    uint8_t opt_name = buffer[offset++];

    uint16_t opt_type = read16();
    uint16_t opt_class = read16();

    uint8_t extended_rcode = buffer[offset++];
    uint8_t edns_version = buffer[offset++];

    uint16_t opt_flags = read16();

    uint16_t rd_length = read16();

    if (rd_length > 0)
    {
        offset += rd_length;
    }

    std::cout << "=== OPT ===" << std::endl;
    std::cout << "Opt Name: " << opt_name << std::endl;
    std::cout << "Opt Type: " << opt_type << std::endl;
    std::cout << "Opt Class: " << opt_class << std::endl;

    std::cout << "Opt RCODE: " << extended_rcode << std::endl;
    std::cout << "Opt EDNS Version: " << edns_version << std::endl;
    std::cout << "Opt Flags: " << opt_flags << std::endl;

    std::cout << "RD Length: " << rd_length << std::endl;

    // Response

    std::vector<uint8_t> response;

    write16(response, transcation_id); // Transaction ID

    // Flags
    uint16_t response_flags = 0;
    response_flags |= 0x8000; // QR = 1 (response)
    response_flags |= 0x0400; // AA = 1 (authoritative)

    if (flags & 0x0100) // RD
        response_flags |= 0x0100;

    write16(response, response_flags);

    write16(response, 1); // QDCOUNT
    write16(response, 1); // ANCOUNT
    write16(response, 0); // NSCOUNT
    write16(response, 0); // ARCOUNT

    response.insert(response.end(), buffer + 12, buffer + question_end);

    write16(response, 0xC00C);
    write16(response, 1);  // A
    write16(response, 1);  // IN
    write32(response, 60); // 60 Seconds
    write16(response, 4);

    response.push_back(1);
    response.push_back(2);
    response.push_back(3);
    response.push_back(4);

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