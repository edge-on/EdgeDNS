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

namespace IpGroupEntry
{
    const size_t MAX_DATA_RECORDS = 100000; // 67.108.864
    const size_t HASH_TABLE_SIZE = 100000; // 67.108.864

    struct __attribute__((packed)) IndexBucket
    {
        // Group ID
        uint64_t group_id_hash; // 16 Byte
        char country_code[8];   // 8 Byte
        int64_t head_slot_idx;  // 8 Byte
    }; // 32 Byte

    struct __attribute__((packed)) PriorityEntry
    {
        uint64_t ipEntryIdx; // 16 Byte
    }; // 16 Byte

    struct __attribute__((packed)) IpGroupEntry
    {
        CassUuid group_id;          // 16 Byte
        char country_code[11];      // 11 Byte
        std::array<uint8_t, 16> ip; // 16 Byte
        int len;                    // 4 Byte
        int priority;               // 4 Byte
        bool is_used;               // 1 Byte
        int32_t next_index = -1;    // 4 byte
    }; // 56 Byte

    struct IpGroupEntryResponse
    {
        std::vector<uint8_t> ip;
        int priority;
    };

    class Mmap
    {
    private:
        char *mmap_base = nullptr;
        IndexBucket *hash_table = nullptr;
        IpGroupEntry *data_entries = nullptr;

        size_t total_file_size = 0;
        int32_t free_list_head_idx = -1;

        uint64_t calculate_hash_from_uuid(const CassUuid &uuid) const;
        int32_t pop_free_slot();
        void push_free_slot(int32_t slotidx);
        size_t find_bucket(uint64_t hash, const char country_code[8], const CassUuid &groupId) const;

    public:
        bool init(const char *filepath);

        bool get_record(const CassUuid group_id, char country_code[8], std::vector<IpGroupEntryResponse> &out_entries);
        bool append_record(CassUuid groupId, char countryCode[8], std::vector<uint8_t> val, int priority);
        bool delete_record(const CassUuid group_id, char country_code[8], int priority);

        ~Mmap();
    };
}