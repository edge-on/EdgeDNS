#include "Core/Uring/BufferPool.hpp"

BufferPool::~BufferPool()
{
    if (buffer_base)
    {
        free(buffer_base);
    }
}
void BufferPool::setup(struct io_uring *ring, size_t count, size_t size, int grp_id)
{
    ring_ptr = ring;
    buf_count = count;
    buf_size = size;
    group_id = grp_id;
    if (posix_memalign(&buffer_base, 4096, buf_count * buf_size) != 0)
    {
        throw std::runtime_error("Buffer pool posix_memalign is accourded!");
    }
    size_t ring_mem_size = count * sizeof(struct io_uring_buf);
    void *ring_mem = nullptr;
    if (posix_memalign(&ring_mem, 4096, ring_mem_size) != 0)
    {
        free(buffer_base);
        throw std::runtime_error("Ring memory posix_memalign failed!");
    }
    buf_ring = (struct io_uring_buf_ring *)ring_mem;
    struct io_uring_buf_reg reg{};
    reg.ring_addr = (uint64_t)buf_ring;
    reg.ring_entries = buf_count;
    reg.bgid = group_id;
    int ret = io_uring_register_buf_ring(ring_ptr, &reg, 0);
    if (ret < 0)
    {
        free(ring_mem);
        free(buffer_base);
        throw std::runtime_error("io_uring_register_buf_ring failed! Error: " + std::to_string(ret));
    }

    for (size_t i = 0; i < buf_count; i++)
    {
        char *buf_ptr = (char *)buffer_base + (i * buf_size);
        io_uring_buf_ring_add(buf_ring, buf_ptr, buf_size, (unsigned short)i,
                              io_uring_buf_ring_mask(buf_count), (int)i);
    }
    io_uring_buf_ring_advance(buf_ring, buf_count);
}

size_t BufferPool::getBufferSize() const
{
    return buf_size;
}

char *BufferPool::getBufferAddress(int buf_id) const
{
    return (char *)buffer_base + (buf_id * buf_size);
}

void BufferPool::releaseBuffer(int buf_id)
{
    char *buf_ptr = (char *)buffer_base + (buf_id * buf_size);
    io_uring_buf_ring_add(buf_ring, buf_ptr, buf_size, (unsigned short)buf_id,
                          io_uring_buf_ring_mask(buf_count), 0);
    io_uring_buf_ring_advance(buf_ring, 1);
}