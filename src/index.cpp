#include "index.hpp"

int main()
{
    EoD* eod = new EoD();
    eod->start();

    return 0;
}

/*
    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0)
    {
        perror("socket");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8901);

    if (bind(udp_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }

    std::cout << "DNS server listening on 8901...\n";

    while (true)
    {
        uint8_t buffer[4096];

        sockaddr_in client{};
        socklen_t len = sizeof(client);

        size_t received = recvfrom(udp_fd, buffer, sizeof(buffer), 0, (sockaddr *)&client, &len);

        size_t offset = 0;

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

        sendto(udp_fd, response.data(), response.size(), 0,
               (sockaddr *)&client, len);
    }

    close(udp_fd);
    */