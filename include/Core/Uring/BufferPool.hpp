#pragma once
#include <liburing.h>
#include <sys/mman.h>
#include <cstdlib>
#include <stdexcept>

class BufferPool
{
private:
    struct io_uring *ring_ptr = nullptr;
    struct io_uring_buf_ring *buf_ring = nullptr;
    void *buffer_base = nullptr;

    size_t buf_count = 0;
    size_t buf_size = 0;
    int group_id = 0;

public:
    BufferPool() = default;

    BufferPool(const BufferPool &) = delete;
    BufferPool &operator=(const BufferPool &) = delete;

    ~BufferPool()
    {
        if (buffer_base)
        {
            free(buffer_base);
        }
    }

    void setup(struct io_uring *ring, size_t count, size_t size, int grp_id)
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
            uint16_t mask = io_uring_buf_ring_mask(buf_count);

            struct io_uring_buf *buf = &buf_ring->bufs[i & mask];
            buf->addr = (uint64_t)buf_ptr;
            buf->len = buf_size;
            buf->bid = i;
        }
        
        buf_ring->tail += buf_count;
    }

    size_t getBufferSize() const { return buf_size; }

    char *getBufferAddress(int buf_id) const
    {
        return (char *)buffer_base + (buf_id * buf_size);
    }

    void releaseBuffer(int buf_id)
    {
        char *buf_ptr = (char *)buffer_base + (buf_id * buf_size);

        uint16_t tail = buf_ring->tail;
        uint16_t mask = io_uring_buf_ring_mask(buf_count);

        struct io_uring_buf *buf = &buf_ring->bufs[tail & mask];
        buf->addr = (uint64_t)buf_ptr;
        buf->len = buf_size;
        buf->bid = buf_id;

        buf_ring->tail++;
    }
};