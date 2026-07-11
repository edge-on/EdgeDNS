#include "Core/Uring/Pipeline.hpp"

void Pipeline::init(int t)
{
    thread = &Gen::activeThreads[t];

    pool = new BufferPool();
    pool->setup(&thread->ring, 1024 * 1024, 2048, 1, 32768);
}

void Pipeline::queueMultishotAccept(int fd)
{
    struct io_uring_sqe *sqe = getSqe();
    if (!sqe)
        return;

    uint64_t data = ((uint64_t)Gen::STATE_MULTISHOT_ACCEPT << 32) | (uint32_t)fd;
    io_uring_prep_multishot_accept(sqe, fd, nullptr, nullptr, 0);
    io_uring_sqe_set_data(sqe, (void *)data);
}

void Pipeline::queueRead(Gen::Connection &conn)
{
    struct io_uring_sqe *sqe = getSqe();
    if (!sqe)
    {
        return;
    }

    uint64_t data = ((uint64_t)Gen::STATE_READ << 32) | (uint32_t)conn.fd;

    conn.msgHdr.msg_namelen = sizeof(struct sockaddr_storage);
    conn.msgHdr.msg_controllen = 0;

    conn.buf_group = pool->pickGroup();

    io_uring_prep_recvmsg_multishot(sqe, conn.fd, &conn.msgHdr, 0);
    sqe->flags |= IOSQE_BUFFER_SELECT;
    sqe->buf_group = (uint16_t)conn.buf_group;
    io_uring_sqe_set_data(sqe, (void *)data);
}

void Pipeline::queueWriteUdp(Gen::Context *ctx)
{
    struct io_uring_sqe *sqe = getSqe();
    if (!sqe)
    {
        delete ctx;
        return;
    }

    ctx->msgHdr.msg_name = &ctx->peerAddr;
    ctx->msgHdr.msg_namelen = ctx->peerLen;
    ctx->msgHdr.msg_iov = &ctx->iov;
    ctx->msgHdr.msg_iovlen = 1;
    ctx->msgHdr.msg_control = nullptr;
    ctx->msgHdr.msg_controllen = 0;

    uint64_t data = (uint64_t)ctx | (1ULL << 63);
    io_uring_prep_sendmsg(sqe, ctx->fd, &ctx->msgHdr, 0);
    io_uring_sqe_set_data(sqe, (void *)data);
}

void Pipeline::queueWriteTcp(Gen::Connection &conn)
{
    struct io_uring_sqe *sqe = getSqe();
    if (!sqe)
        return;

    uint64_t data = ((uint64_t)Gen::STATE_WRITE << 32) | (uint32_t)conn.fd;
    io_uring_prep_send(sqe, conn.fd, conn.writeBuffer, conn.len, 0);
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