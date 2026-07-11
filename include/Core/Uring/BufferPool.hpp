#pragma once

#include <liburing.h>
#include <vector>
#include <cstddef>
#include <atomic>
#include <cassert>
#include <stdexcept>
#include <cstdlib>
#include <cstring>

class BufferPool
{
public:
    ~BufferPool();

    void setup(struct io_uring *ring, size_t total_count, size_t size,
               int base_group_id, size_t max_per_group = 32768);

    size_t getBufferSize() const;
    char *getBufferAddress(int group_id, int buf_id) const;
    void releaseBuffer(int group_id, int buf_id);

    int pickGroup();

    size_t groupCount() const { return rings.size(); }

private:
    struct RingSlot
    {
        struct io_uring_buf_ring *buf_ring = nullptr;
        void *buffer_base = nullptr;
        int group_id = 0;
        size_t count = 0;
    };

    int indexOf(int group_id) const { return group_id - base_group_id; }

    struct io_uring *ring_ptr = nullptr;
    std::vector<RingSlot> rings;
    size_t buf_size = 0;
    int base_group_id = 0;
    std::atomic<size_t> rr_counter{0};
};