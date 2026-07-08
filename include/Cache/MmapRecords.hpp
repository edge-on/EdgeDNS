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

const size_t MAX_DATA_RECORDS = 50000;
const size_t HASH_TABLE_SIZE = 65536;

struct __attribute__((packed)) IndexBucket
{
    uint64_t name_hash;
    int32_t qtype;
    int32_t head_slot_idx;
}; // 16 Byte

struct __attribute__((packed)) DNSRecord
{
    bool is_used;                     // 1 byte
    int32_t next_index;               // 4 byte
    uint32_t ttl;                     // 4 byte
    uint16_t priority;                // 2 byte
    uint8_t rdata_len;                // 1 byte
    std::array<uint8_t, 252> payload; // 252 byte
}; // 264 Byte

struct DNSResponseData
{
    uint32_t ttl;
    uint16_t priority;
    std::vector<uint8_t> rdata;
};

class Mmap
{
private:
    char *mmap_base = nullptr;
    IndexBucket *hash_table = nullptr;
    DNSRecord *data_records = nullptr;

    size_t total_file_size = 0;
    int32_t free_list_head_idx = -1;

    uint64_t calculate_hash(const std::vector<uint8_t> &wire_name) const;
    int32_t pop_free_slot();
    void push_free_slot(int32_t slotidx);
    size_t find_bucket(uint64_t hash, int32_t qtype) const;

public:
    bool init(const char *filepath);

    bool get_record(const std::vector<uint8_t> &wire_name, int32_t qtype, std::vector<DNSResponseData> &out_records);
    bool append_record(const std::vector<uint8_t> &wire_name, int32_t qtype, uint32_t ttl, uint16_t priority, const std::vector<uint8_t> &binary_rdata);
    bool delete_record(const std::vector<uint8_t> &wire_name, int32_t qtype);

    ~Mmap();
};