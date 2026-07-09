#pragma once

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <array>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <cassandra.h>

namespace System
{
    const size_t MAX_DATA_RECORDS = 2;

    typedef enum
    {
        RECORDS,
        IP_GROUPS
    };

    struct __attribute__((packed)) SystemMetadata
    {
        uint64_t type;    // 8 Bytes
        CassUuid version; // 16 Byte
    }; // 24 Byte

    class Mmap
    {
    private:
        char *mmap_base = nullptr;
        SystemMetadata *system_metadata = nullptr;

        size_t total_file_size = 0;
        int32_t free_list_head_idx = -1;

        void push_free_slot(int32_t slotidx);
        size_t find_bucket(uint64_t hash, int32_t qtype) const;

    public:
        bool init(const char *filepath);

        bool get_record(const int type, CassUuid &version);
        bool append_record(const int type, CassUuid version);
        bool delete_record(const int type);

        ~Mmap();
    };
}