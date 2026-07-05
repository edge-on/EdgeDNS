#include "Core/Uring/Pipeline.hpp"

void Pipeline::init(int thread)
{
    this->thread = &Gen::activeThreads[thread];
}

void Pipeline::queueMultishotAccept(int fd)
{
    struct io_uring_sqe *sqe = getSqe();
    if (!sqe)
        return;
}

struct io_uring_sqe *Pipeline::getSqe()
{
    if (!thread->ring)
        return nullptr;

    struct io_uring_sqe *sqe = io_uring_get_sqe(thread->ring);
    if (!sqe)
    {
        io_uring_submit(thread->ring);
        sqe = io_uring_get_sqe(thread->ring);
    }

    return sqe;
}