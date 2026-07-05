#pragma once

#include <liburing.h>
#include <iostream>

#include "Core/Uring/BufferPool.hpp"
#include "Core/Gen/Gen.hpp"

class Pipeline
{
public:
    void init(int thread);

    void queueMultishotAccept(int fd);
    void queueRead(Gen::Connection &conn);
    void queueWrite(Gen::Context *ctx);

    BufferPool *pool;
private:
    struct io_uring_sqe *getSqe();
    Gen::Thread *thread;
};