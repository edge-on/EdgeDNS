#include "Cache/MmapRecords.hpp"

Records::Mmap::~Mmap()
{
    if (mmap_base)
        munmap(mmap_base, total_file_size);
}

bool Records::Mmap::init(const char *filepath)
{
    size_t index_zone_size = HASH_TABLE_SIZE * sizeof(IndexBucket);
    size_t data_zone_size = MAX_DATA_RECORDS * sizeof(DNSRecord);
    total_file_size = index_zone_size + data_zone_size;

    bool is_new_file = (access(filepath, F_OK) == -1);

    int fd = open(filepath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1)
        return false;

    if (ftruncate(fd, total_file_size) == -1)
    {
        close(fd);
        return false;
    }

    void *map = mmap(nullptr, total_file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    if (map == MAP_FAILED)
        return false;

    mmap_base = static_cast<char *>(map);

    hash_table = reinterpret_cast<IndexBucket *>(mmap_base);
    data_records = reinterpret_cast<DNSRecord *>(mmap_base + index_zone_size);

    if (is_new_file)
    {
        std::memset(hash_table, 0, index_zone_size);

        free_list_head_idx = 0;
        for (size_t i = 0; i < MAX_DATA_RECORDS; ++i)
        {
            data_records[i].is_used = false;
            data_records[i].next_index = (i == MAX_DATA_RECORDS - 1) ? -1 : static_cast<int32_t>(i + 1);
            data_records[i].ttl = 0;
            data_records[i].priority = 0;
            data_records[i].rdata_len = 0;
            std::memset(data_records[i].payload.data(), 0, data_records[i].payload.size());
        }
    }
    else
    {
        free_list_head_idx = -1;
        for (int32_t i = static_cast<int32_t>(MAX_DATA_RECORDS) - 1; i >= 0; --i)
        {
            if (!data_records[i].is_used)
            {
                data_records[i].next_index = free_list_head_idx;
                free_list_head_idx = i;
            }
        }
    }

    return true;
}

bool Records::Mmap::get_record(const std::vector<uint8_t> &wire_name, int32_t qtype, std::vector<DNSResponseData> &out_records)
{
    out_records.clear();
    uint64_t hash = calculate_hash(wire_name);
    size_t bucket_idx = find_bucket(hash, qtype);

    if (bucket_idx == -1 || hash_table[bucket_idx].name_hash == 0)
    {
        return false;
    }

    int32_t current_slot = hash_table[bucket_idx].head_slot_idx;
    while (current_slot >= 0 && current_slot < static_cast<int32_t>(MAX_DATA_RECORDS))
    {
        if (!data_records[current_slot].is_used)
            break;

        DNSResponseData node;
        node.ttl = data_records[current_slot].ttl;
        node.priority = data_records[current_slot].priority;

        uint8_t len = data_records[current_slot].rdata_len;
        node.rdata.assign(data_records[current_slot].payload.begin(), data_records[current_slot].payload.begin() + len);

        out_records.push_back(node);
        current_slot = data_records[current_slot].next_index;
    }
    return !out_records.empty();
}

bool Records::Mmap::append_record(const std::vector<uint8_t> &wire_name, int32_t qtype, uint32_t ttl, uint16_t priority, bool isGeo, const std::vector<uint8_t> &binary_rdata)
{
    if (binary_rdata.size() > data_records[0].payload.size())
    {
        std::cerr << "ERROR: RData size cannot be bigger than 252 byte!\n";
        return false;
    }

    uint64_t hash = calculate_hash(wire_name);
    size_t bucket_idx = find_bucket(hash, qtype);

    if (bucket_idx == -1)
        return false;

    int32_t new_slot_idx = pop_free_slot();
    if (new_slot_idx == -1)
        return false;

    data_records[new_slot_idx].ttl = ttl;
    data_records[new_slot_idx].priority = priority;
    data_records[new_slot_idx].is_geo = isGeo;
    data_records[new_slot_idx].rdata_len = static_cast<uint8_t>(binary_rdata.size());
    std::memcpy(data_records[new_slot_idx].payload.data(), binary_rdata.data(), binary_rdata.size());

    if (hash_table[bucket_idx].name_hash == 0)
    {
        hash_table[bucket_idx].name_hash = hash;
        hash_table[bucket_idx].qtype = qtype;
        hash_table[bucket_idx].head_slot_idx = new_slot_idx;
        data_records[new_slot_idx].next_index = -1;
    }
    else
    {
        data_records[new_slot_idx].next_index = hash_table[bucket_idx].head_slot_idx;
        hash_table[bucket_idx].head_slot_idx = new_slot_idx;
    }
    return true;
}

bool Records::Mmap::delete_record(const std::vector<uint8_t> &wire_name, int32_t qtype)
{
    uint64_t hash = calculate_hash(wire_name);
    size_t bucket_idx = find_bucket(hash, qtype);

    if (bucket_idx == -1 || hash_table[bucket_idx].name_hash == 0)
    {
        return false;
    }

    int32_t current_slot = hash_table[bucket_idx].head_slot_idx;
    while (current_slot >= 0 && current_slot < static_cast<int32_t>(MAX_DATA_RECORDS))
    {
        int32_t next = data_records[current_slot].next_index;
        push_free_slot(current_slot);
        current_slot = next;
    }

    hash_table[bucket_idx].name_hash = 0;
    hash_table[bucket_idx].qtype = 0;
    hash_table[bucket_idx].head_slot_idx = -1;

    return true;
}

uint64_t Records::Mmap::calculate_hash(const std::vector<uint8_t> &wire_name) const
{
    uint64_t hash = 14695981039346656037ULL;
    for (uint8_t byte : wire_name)
    {
        if (byte >= 65 && byte <= 90)
        {
            byte += 32;
        }
        hash ^= static_cast<uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

int32_t Records::Mmap::pop_free_slot()
{
    if (free_list_head_idx == -1)
        return -1;

    int32_t allocated_idx = free_list_head_idx;
    free_list_head_idx = data_records[allocated_idx].next_index;

    data_records[allocated_idx].is_used = true;
    data_records[allocated_idx].next_index = -1;
    return allocated_idx;
}

void Records::Mmap::push_free_slot(int32_t slotidx)
{
    data_records[slotidx].is_used = false;
    std::memset(data_records[slotidx].payload.data(), 0, data_records[slotidx].payload.size());
    data_records[slotidx].ttl = 0;
    data_records[slotidx].priority = 0;
    data_records[slotidx].rdata_len = 0;
    data_records[slotidx].next_index = free_list_head_idx;
    free_list_head_idx = slotidx;
}

size_t Records::Mmap::find_bucket(uint64_t hash, int32_t qtype) const
{
    size_t index = hash & (HASH_TABLE_SIZE - 1);
    size_t start_index = index;

    while (true)
    {
        if (hash_table[index].name_hash == 0)
        {
            return index;
        }

        if (hash_table[index].name_hash == hash && hash_table[index].qtype == qtype)
        {
            return index;
        }

        index = (index + 1) & (HASH_TABLE_SIZE - 1);
        if (index == start_index)
        {
            break;
        }
    }
    return -1;
}