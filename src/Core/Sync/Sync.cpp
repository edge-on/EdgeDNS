#include "Core/Sync/Sync.hpp"

int Sync::initSync(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    Utils::Socket::makeNonBlocking(fd);

    sockaddr_in addr{};
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;

    if (bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0)
        perror("bind error");

    if (listen(fd, SOMAXCONN) < 0)
        perror("listen error");

    return fd;
}