#include "Core/Uring/BufferPool.hpp"

BufferPool::~BufferPool()
{
    for (auto &slot : rings)
    {
        if (ring_ptr && slot.buf_ring)
        {
            io_uring_unregister_buf_ring(ring_ptr, slot.group_id);
        }
        if (slot.buf_ring)
        {
            free(slot.buf_ring);
        }
        if (slot.buffer_base)
        {
            free(slot.buffer_base);
        }
    }
}

void BufferPool::setup(struct io_uring *ring, size_t total_count, size_t size, int base_grp_id, size_t max_per_group)
{
    if (total_count == 0)
    {
        throw std::runtime_error("total_count must be > 0");
    }

    if (max_per_group == 0 || (max_per_group & (max_per_group - 1)) != 0)
    {
        throw std::runtime_error("max_per_group must be a power of two!");
    }

    ring_ptr = ring;
    buf_size = size;
    base_group_id = base_grp_id;

    size_t remaining = total_count;
    int grp = base_grp_id;

    while (remaining > 0)
    {
        size_t this_count = std::min(remaining, max_per_group);

        size_t pow2 = 1;
        while (pow2 * 2 <= this_count)
            pow2 *= 2;
        this_count = pow2;
        if (this_count == 0)
            break;

        RingSlot slot;
        slot.group_id = grp;
        slot.count = this_count;

        if (posix_memalign(&slot.buffer_base, 4096, this_count * buf_size) != 0)
        {
            throw std::runtime_error("Buffer pool posix_memalign failed!");
        }

        size_t ring_mem_size = this_count * sizeof(struct io_uring_buf);
        void *ring_mem = nullptr;
        if (posix_memalign(&ring_mem, 4096, ring_mem_size) != 0)
        {
            free(slot.buffer_base);
            throw std::runtime_error("Ring memory posix_memalign failed!");
        }

        slot.buf_ring = (struct io_uring_buf_ring *)ring_mem;
        struct io_uring_buf_reg reg{};
        reg.ring_addr = (uint64_t)slot.buf_ring;
        reg.ring_entries = this_count;
        reg.bgid = slot.group_id;

        int ret = io_uring_register_buf_ring(ring_ptr, &reg, 0);
        if (ret < 0)
        {
            free(ring_mem);
            free(slot.buffer_base);
            throw std::runtime_error("io_uring_register_buf_ring failed! Error: " + std::to_string(ret));
        }

        unsigned mask = io_uring_buf_ring_mask(this_count);
        for (size_t i = 0; i < this_count; i++)
        {
            char *buf_ptr = (char *)slot.buffer_base + (i * buf_size);
            io_uring_buf_ring_add(slot.buf_ring, buf_ptr, buf_size, (unsigned short)i, mask, (int)i);
        }
        io_uring_buf_ring_advance(slot.buf_ring, this_count);

        rings.push_back(slot);

        remaining -= this_count;
        grp++;
    }
}

size_t BufferPool::getBufferSize() const
{
    return buf_size;
}

char *BufferPool::getBufferAddress(int group_id, int buf_id) const
{
    int idx = indexOf(group_id);
    assert(idx >= 0 && (size_t)idx < rings.size());
    const RingSlot &slot = rings[idx];
    assert(buf_id >= 0 && (size_t)buf_id < slot.count);
    return (char *)slot.buffer_base + (buf_id * buf_size);
}

void BufferPool::releaseBuffer(int group_id, int buf_id)
{
    int idx = indexOf(group_id);
    assert(idx >= 0 && (size_t)idx < rings.size());
    RingSlot &slot = rings[idx];
    assert(buf_id >= 0 && (size_t)buf_id < slot.count);

    char *buf_ptr = (char *)slot.buffer_base + (buf_id * buf_size);
    io_uring_buf_ring_add(slot.buf_ring, buf_ptr, buf_size, (unsigned short)buf_id,
                          io_uring_buf_ring_mask(slot.count), 0);
    io_uring_buf_ring_advance(slot.buf_ring, 1);
}

int BufferPool::pickGroup()
{
    size_t n = rr_counter.fetch_add(1, std::memory_order_relaxed);
    return base_group_id + (int)(n % rings.size());
}