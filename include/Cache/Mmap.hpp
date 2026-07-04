#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <array>
#include <unordered_map>
#include <cstring>
#include <chrono>
#include <string>
#include <vector>

const size_t RECORD_SIZE = 264;
const size_t MAX_RECORDS = 1000;
const size_t FILE_SIZE = RECORD_SIZE * MAX_RECORDS;

struct __attribute__((packed)) DNSRecord
{
    bool is_used;                  // 1 byte
    int32_t next_index;            // 4 byte
    std::array<char, 259> payload; // 259 byte
}; // Tam 264 byte

class Mmap
{
private:
    DNSRecord *records = nullptr;
    std::vector<uint32_t> free_list;

    int32_t find_free_slot()
    {
        if (free_list.empty())
            return -1;

        int32_t idx = free_list.back();
        free_list.pop_back();
        return idx;
    }

public:
    bool init(const char *filepath)
    {
        int fd = open(filepath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd == -1)
            return false;

        if (ftruncate(fd, FILE_SIZE) == -1)
        {
            close(fd);
            return false;
        }

        void *map = mmap(nullptr, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);

        if (map == MAP_FAILED)
            return false;

        records = static_cast<DNSRecord *>(map);
        free_list.reserve(MAX_RECORDS);

        for (int i = 0; i < MAX_RECORDS; ++i)
        {
            records[i].is_used = false;
            records[i].next_index = -2;

            free_list.push_back(i);
        }

        return true;
    }

    bool get_record(int32_t slotidx, std::string &out_full_payload)
    {
        std::string assembled_data = "";

        while (slotidx >= 0 && slotidx < static_cast<int32_t>(MAX_RECORDS))
        {
            assembled_data.append(records[slotidx].payload.data());
            slotidx = records[slotidx].next_index;
        }

        out_full_payload = assembled_data;
        return true;
    }

    void delete_record_internal(int32_t slotidx)
    {
        while (slotidx >= 0 && slotidx < static_cast<int32_t>(MAX_RECORDS))
        {
            int32_t next = records[slotidx].next_index;

            records[slotidx].is_used = false;
            records[slotidx].next_index = -2;
            std::memset(records[slotidx].payload.data(), 0, 259);

            free_list.push_back(slotidx);

            slotidx = next;
        }
    }

    int32_t set_record_immutable(int32_t old_slotidx, const std::string &full_payload)
    {
        if (old_slotidx >= 0)
        {
            delete_record_internal(old_slotidx);
        }

        size_t current_str_pos = 0;
        int32_t previous_slot_idx = -1;
        int32_t first_slot_idx = -1;

        const size_t CHUNK_SIZE = 258;

        while (current_str_pos < full_payload.length())
        {
            int32_t current_slot_idx = find_free_slot();
            if (current_slot_idx == -1)
            {
                std::cerr << "ERROR: mmap is full!\n";
                return -1;
            }

            records[current_slot_idx].is_used = true;
            records[current_slot_idx].next_index = -1;

            std::string chunk = full_payload.substr(current_str_pos, CHUNK_SIZE);
            std::strncpy(records[current_slot_idx].payload.data(), chunk.c_str(), CHUNK_SIZE);
            records[current_slot_idx].payload[CHUNK_SIZE] = '\0';

            if (previous_slot_idx != -1)
            {
                records[previous_slot_idx].next_index = current_slot_idx;
            }

            if (first_slot_idx == -1)
            {
                first_slot_idx = current_slot_idx;
            }

            previous_slot_idx = current_slot_idx;
            current_str_pos += chunk.length();
        }

        return first_slot_idx;
    }

    ~Mmap()
    {
        if (records)
            munmap(records, FILE_SIZE);
    }
};