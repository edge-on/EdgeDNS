#pragma once

#include <liburing.h>

#include "Core/Gen/Gen.hpp"

class Pipeline
{
public:
    void init(int thread);

    void queueMultishotAccept(int fd);
    void queueRead(Gen::Connection &conn);
    void queueWrite(Gen::Connection &conn);

private:
    struct io_uring_sqe *getSqe();

    Gen::Thread *thread;
};