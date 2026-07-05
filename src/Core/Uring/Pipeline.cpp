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

    uint64_t data = (uint64_t(Gen::STATE_MULTISHOT_ACCEPT)) | (uint32_t)fd;
    io_uring_prep_multishot_accept(sqe, fd, nullptr, nullptr, 0);
    io_uring_sqe_set_data(sqe, (void *)data);
}

void Pipeline::queueRead(Gen::Connection &conn)
{
    struct io_uring_sqe *sqe = getSqe();
    if (!sqe)
        return;

    uint64_t data = (uint64_t(Gen::STATE_MULTISHOT_ACCEPT)) | (uint32_t)conn.fd;
    io_uring_prep_recv(sqe, conn.fd, conn.readBuffer.data(), 65535, 0);
    io_uring_sqe_set_data(sqe, (void *)data);
}

void Pipeline::queueWrite(Gen::Connection &conn)
{
    struct io_uring_sqe *sqe = getSqe();
    if (!sqe)
        return;

    uint64_t data = (uint64_t(Gen::STATE_MULTISHOT_ACCEPT)) | (uint32_t)conn.fd;
    
    io_uring_sqe_set_data(sqe, (void *)data);
}

struct io_uring_sqe *Pipeline::getSqe()
{
    if (!&thread->ring)
        return nullptr;

    struct io_uring_sqe *sqe = io_uring_get_sqe(&thread->ring);
    if (!sqe)
    {
        io_uring_submit(&thread->ring);
        sqe = io_uring_get_sqe(&thread->ring);
    }

    return sqe;
}