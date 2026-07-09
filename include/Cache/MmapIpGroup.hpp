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

const size_t MAX_DATA_RECORDS = 50000;
const size_t HASH_TABLE_SIZE = 65536;

struct __attribute__((packed)) IndexBucket
{
    // Group ID
    uint64_t time_and_version;   // 8 Byte
    uint64_t clock_seq_and_node; // 8 Byte
    int32_t head_slot_idx;       // 4 Byte
}; // 16 Byte

struct __attribute__((packed)) IpGroupEntries
{
    CassUuid version;    // 16 Byte
    CassUuid group_id;   // 16 Byte
    CassUuid id;         // 16 Byte
    char countryCode[8]; // 8 Byte
    uint8_t ip[4];       // 4 Byte
    int priority;        // 4 Byte
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

    size_t total_file_size = 0;
    int32_t free_list_head_idx = -1;

    uint64_t calculate_hash(const std::vector<uint8_t> &wire_name) const;
    int32_t pop_free_slot();
    void push_free_slot(int32_t slotidx);
    size_t find_bucket(uint64_t hash, int32_t qtype) const;

public:
    bool init(const char *filepath);

    bool get_record(const CassUuid uuid, int32_t qtype, std::vector<DNSResponseData> &out_records);
    bool append_record(const CassUuid uuid, int32_t qtype, uint32_t ttl, uint16_t priority, const std::vector<uint8_t> &binary_rdata);
    bool delete_record(const CassUuid uuid);

    ~Mmap();
};