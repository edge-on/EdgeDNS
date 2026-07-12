#pragma once

#include <fcntl.h>

namespace Utils
{
    class Socket
    {
    public:
        static int makeNonBlocking(int sfd)
        {
            int flags = fcntl(sfd, F_GETFL, 0);
            if (flags == -1)
                return -1;
            flags |= O_NONBLOCK;
            return fcntl(sfd, F_SETFL, flags);
        }
    };
} // namespace Utils
