#pragma once

#include <liburing.h>

#include "Core/Gen/Gen.hpp"

class Pipeline
{
public:
    void init(int thread);

    void queueMultishotAccept(int fd);

private:
    struct io_uring_sqe *getSqe();

    Gen::Thread *thread;
};