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
    ~BufferPool();

    void setup(struct io_uring *ring, size_t count, size_t size, int grp_id);
    size_t getBufferSize() const;
    char *getBufferAddress(int buf_id) const;
    void releaseBuffer(int buf_id);
};