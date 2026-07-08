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

const size_t MAX_DATA_RECORDS = 2;

typedef enum
{
    RECORDS,
    IP_GROUPS
};

struct __attribute__((packed)) System
{
    uint64_t type;         // 8 Bytes
    CassUuid version; // 16 Byte
}; // 24 Byte

class Mmap
{
private:
    char *mmap_base = nullptr;
    IndexBucket *hash_table = nullptr;
    DNSRecord *data_records = nullptr;

    size_t total_file_size = 0;
    int32_t free_list_head_idx = -1;

    int32_t pop_free_slot();
    void push_free_slot(int32_t slotidx);
    size_t find_bucket(uint64_t hash, int32_t qtype) const;

public:
    bool init(const char *filepath);

    bool get_record(const int type, CassUuid &version);
    bool append_record(const int type, CassUuid version);
    bool delete_record(const int type);

    ~Mmap();
};