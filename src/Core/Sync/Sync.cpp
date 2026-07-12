#include "Core/Sync/Sync.hpp"

void Sync::initSync(int port, Gen::Thread &thread)
{
    thread.syncFd = socket(AF_INET, SOCK_STREAM, 0);

    Utils::Socket::makeNonBlocking(thread.syncFd);

    int opt = 1;
    setsockopt(thread.syncFd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;

    if (bind(thread.syncFd, (sockaddr *)&addr, sizeof(addr)) < 0)
        perror("bind error");

    if (listen(thread.syncFd, SOMAXCONN) < 0)
        perror("listen error");
}