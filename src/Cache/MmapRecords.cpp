#include "Cache/MmapRecords.hpp"

Records::Mmap::~Mmap()
{
    if (mmap_base)
        munmap(mmap_base, total_file_size);
}

bool Records::Mmap::init(const char *filepath)
{
    size_t index_zone_size = HASH_TABLE_SIZE * sizeof(IndexBucket);
    size_t id_zone_size = ID_HASH_TABLE_SIZE * sizeof(IDBucket);
    size_t data_zone_size = MAX_DATA_RECORDS * sizeof(DNSRecord);

    total_file_size = index_zone_size + id_zone_size + data_zone_size;

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
    id_hash_table = reinterpret_cast<IDBucket *>(mmap_base + index_zone_size);
    data_records = reinterpret_cast<DNSRecord *>(mmap_base + index_zone_size + id_zone_size);

    if (is_new_file)
    {
        std::memset(hash_table, 0, index_zone_size);
        std::memset(id_hash_table, 0, id_zone_size);

        for (size_t i = 0; i < MAX_DATA_RECORDS; ++i)
        {
            data_records[i].is_used = false;
            data_records[i].next_index = -1;
            data_records[i].prev_index = -1;
            data_records[i].ttl = 0;
            data_records[i].priority = 0;
            data_records[i].rdata_len = 0;
            std::memset(data_records[i].payload.data(), 0, data_records[i].payload.size());
        }

        for (int32_t i = 0; i < ID_HASH_TABLE_SIZE; ++i)
            id_hash_table[i].slot_idx = -1;
    }
    else
    {
        for (int32_t i = 0; i < ID_HASH_TABLE_SIZE; ++i)
            id_hash_table[i].slot_idx = -1;

        for (int32_t i = 0; i < static_cast<int32_t>(MAX_DATA_RECORDS); ++i)
        {
            if (data_records[i].is_used)
            {
                uint64_t id_hash = calculate_id_hash(data_records[i].id) & (ID_HASH_TABLE_SIZE - 1);
                id_hash_table[id_hash].slot_idx = i;
            }
        }
    }

    free_list_head_idx = -1;
    for (int32_t i = static_cast<int32_t>(MAX_DATA_RECORDS) - 1; i >= 0; --i)
    {
        if (!data_records[i].is_used)
        {
            data_records[i].next_index = free_list_head_idx;
            free_list_head_idx = i;
        }
    }

    return true;
}

bool Records::Mmap::get_record(const uint8_t *wire_name, size_t wire_len, int32_t qtype, std::vector<DNSResponseData> &out_records)
{
    out_records.clear();
    uint64_t hash = calculate_hash(wire_name, wire_len);
    size_t bucket_idx = find_bucket(hash, qtype);

    if (bucket_idx == static_cast<size_t>(-1) || hash_table[bucket_idx].name_hash == 0)
    {
        return false;
    }

    int32_t current_slot = hash_table[bucket_idx].head_slot_idx;
    size_t visited = 0;
    while (current_slot >= 0 && current_slot < static_cast<int32_t>(MAX_DATA_RECORDS))
    {
        if (!data_records[current_slot].is_used)
            break;

        if (++visited > MAX_DATA_RECORDS)
            break;

        DNSResponseData node;
        node.ttl = data_records[current_slot].ttl;
        node.priority = data_records[current_slot].priority;
        node.is_geo = data_records[current_slot].is_geo;
        node.group_id = data_records[current_slot].group_id;
        node.id = data_records[current_slot].id;
        node.bucket_idx = data_records[current_slot].bucket_idx;

        if (!node.is_geo)
        {
            uint8_t len = data_records[current_slot].rdata_len;
            node.rdata.assign(data_records[current_slot].payload.begin(),
                              data_records[current_slot].payload.begin() + len);
        }

        out_records.push_back(node);

        int32_t next_slot = data_records[current_slot].next_index;
        if (next_slot == current_slot)
            break;

        current_slot = next_slot;
    }
    return !out_records.empty();
}

bool Records::Mmap::append_record(const std::vector<uint8_t> &wire_name,
                                  int32_t qtype,
                                  uint32_t ttl,
                                  uint16_t priority,
                                  CassUuid group_id,
                                  CassUuid id,
                                  bool is_geo,
                                  const std::vector<uint8_t> &binary_rdata)
{
    if (!is_geo && binary_rdata.size() > data_records[0].payload.size())
    {
        std::cerr << "ERROR: RData size cannot be bigger than 252 byte!\n";
        return false;
    }

    uint64_t hash = calculate_hash(wire_name.data(), wire_name.size());
    size_t bucket_idx = find_bucket(hash, qtype);

    if (bucket_idx == static_cast<size_t>(-1))
        return false;

    uint64_t id_hash = calculate_id_hash(id) & (ID_HASH_TABLE_SIZE - 1);
    if (id_hash_table[id_hash].slot_idx != -1)
        return false;

    int32_t new_slot_idx = pop_free_slot();
    if (new_slot_idx == -1)
        return false;

    id_hash_table[id_hash].slot_idx = new_slot_idx;

    data_records[new_slot_idx].ttl = ttl;
    data_records[new_slot_idx].priority = priority;
    data_records[new_slot_idx].group_id = group_id;
    data_records[new_slot_idx].id = id;
    data_records[new_slot_idx].is_geo = is_geo;
    data_records[new_slot_idx].bucket_idx = bucket_idx;
    data_records[new_slot_idx].is_used = true;

    if (!is_geo)
    {
        std::memset(data_records[new_slot_idx].payload.data(), 0, data_records[new_slot_idx].payload.size());
        data_records[new_slot_idx].rdata_len = static_cast<uint8_t>(binary_rdata.size());
        std::memcpy(data_records[new_slot_idx].payload.data(), binary_rdata.data(), binary_rdata.size());
    }
    else
    {
        data_records[new_slot_idx].rdata_len = 0;
        std::memset(data_records[new_slot_idx].payload.data(), 0, data_records[new_slot_idx].payload.size());
    }

    if (hash_table[bucket_idx].name_hash == 0)
    {
        hash_table[bucket_idx].name_hash = hash;
        hash_table[bucket_idx].qtype = qtype;
        hash_table[bucket_idx].head_slot_idx = new_slot_idx;
        data_records[new_slot_idx].next_index = -1;
        data_records[new_slot_idx].prev_index = -1;
    }
    else
    {
        int32_t old_head = hash_table[bucket_idx].head_slot_idx;

        if (old_head == new_slot_idx || !data_records[old_head].is_used)
        {
            int32_t salvage_head = -1;
            if (old_head != new_slot_idx && old_head >= 0 &&
                old_head < static_cast<int32_t>(MAX_DATA_RECORDS))
            {
                int32_t salvage = data_records[old_head].next_index;
                size_t salvage_visited = 0;
                while (salvage >= 0 && salvage < static_cast<int32_t>(MAX_DATA_RECORDS))
                {
                    if (++salvage_visited > MAX_DATA_RECORDS)
                        break;
                    if (data_records[salvage].is_used)
                    {
                        salvage_head = salvage;
                        break;
                    }
                    salvage = data_records[salvage].next_index;
                }
            }

            hash_table[bucket_idx].head_slot_idx = new_slot_idx;
            data_records[new_slot_idx].next_index = salvage_head;
            data_records[new_slot_idx].prev_index = -1;
            if (salvage_head != -1)
                data_records[salvage_head].prev_index = new_slot_idx;
        }
        else
        {
            data_records[old_head].prev_index = new_slot_idx;
            data_records[new_slot_idx].next_index = old_head;
            data_records[new_slot_idx].prev_index = -1;
            hash_table[bucket_idx].head_slot_idx = new_slot_idx;
        }
    }
    return true;
}

bool Records::Mmap::update_record(CassUuid id, uint32_t ttl, uint16_t priority, CassUuid groupId, bool isGeo, const std::vector<uint8_t> &binary_rdata)
{
    uint64_t id_hash = calculate_id_hash(id) & (ID_HASH_TABLE_SIZE - 1);

    if (id_hash_table[id_hash].slot_idx == -1)
        return false;

    int32_t slot_idx = id_hash_table[id_hash].slot_idx;

    if (!data_records[slot_idx].is_used ||
        data_records[slot_idx].id.clock_seq_and_node != id.clock_seq_and_node ||
        data_records[slot_idx].id.time_and_version != id.time_and_version)
    {
        return false;
    }

    if (!isGeo && binary_rdata.size() > data_records[slot_idx].payload.size())
        return false;

    data_records[slot_idx].ttl = ttl;
    data_records[slot_idx].priority = priority;
    data_records[slot_idx].group_id = groupId;
    data_records[slot_idx].is_geo = isGeo;

    if (!isGeo)
    {
        std::memset(data_records[slot_idx].payload.data(), 0, data_records[slot_idx].payload.size());
        data_records[slot_idx].rdata_len = static_cast<uint8_t>(binary_rdata.size());
        std::memcpy(data_records[slot_idx].payload.data(), binary_rdata.data(), binary_rdata.size());
    }
    else
    {
        data_records[slot_idx].rdata_len = 0;
        std::memset(data_records[slot_idx].payload.data(), 0, data_records[slot_idx].payload.size());
    }

    return true;
}

bool Records::Mmap::delete_record(const std::vector<uint8_t> &wire_name, int32_t qtype)
{
    uint64_t hash = calculate_hash(wire_name.data(), wire_name.size());
    size_t bucket_idx = find_bucket(hash, qtype);

    if (bucket_idx == static_cast<size_t>(-1) || hash_table[bucket_idx].name_hash == 0)
    {
        return false;
    }

    int32_t current_slot = hash_table[bucket_idx].head_slot_idx;
    size_t visited = 0;
    while (current_slot >= 0 && current_slot < static_cast<int32_t>(MAX_DATA_RECORDS))
    {
        if (++visited > MAX_DATA_RECORDS)
            break;

        int32_t next = data_records[current_slot].next_index;
        push_free_slot(current_slot);
        current_slot = next;
    }

    hash_table[bucket_idx].name_hash = 0;
    hash_table[bucket_idx].qtype = 0;
    hash_table[bucket_idx].head_slot_idx = -1;

    return true;
}

bool Records::Mmap::delete_record_from_uuid(CassUuid id)
{
    uint64_t id_hash = calculate_id_hash(id) & (ID_HASH_TABLE_SIZE - 1);

    if (id_hash_table[id_hash].slot_idx == -1)
        return false;

    int32_t slot_idx = id_hash_table[id_hash].slot_idx;

    if (!data_records[slot_idx].is_used ||
        data_records[slot_idx].id.clock_seq_and_node != id.clock_seq_and_node ||
        data_records[slot_idx].id.time_and_version != id.time_and_version)
    {
        return false;
    }

    int32_t bucket_idx = data_records[slot_idx].bucket_idx;
    int32_t prev = data_records[slot_idx].prev_index;
    int32_t next = data_records[slot_idx].next_index;

    if (slot_idx == hash_table[bucket_idx].head_slot_idx)
    {
        hash_table[bucket_idx].head_slot_idx = next;
    }
    else if (prev != -1)
    {
        data_records[prev].next_index = next;
    }

    if (next != -1)
    {
        data_records[next].prev_index = prev;
    }

    if (hash_table[bucket_idx].head_slot_idx == -1)
    {
        hash_table[bucket_idx].name_hash = 0;
        hash_table[bucket_idx].qtype = 0;
    }

    push_free_slot(slot_idx);
    id_hash_table[id_hash].slot_idx = -1;

    return true;
}

uint64_t Records::Mmap::calculate_hash(const uint8_t *wire_name, size_t len) const
{
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i)
    {
        uint8_t byte = wire_name[i];
        if (byte >= 65 && byte <= 90)
        {
            byte += 32;
        }
        hash ^= static_cast<uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint64_t Records::Mmap::calculate_id_hash(CassUuid uuid) const
{
    uint8_t bytes[16];

    std::memcpy(bytes, &uuid.time_and_version, 8);
    std::memcpy(bytes + 8, &uuid.clock_seq_and_node, 8);

    uint64_t hash = 14695981039346656037ULL;
    for (int i = 0; i < 16; ++i)
    {
        uint8_t byte = bytes[i];
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
    data_records[allocated_idx].prev_index = -1;
    return allocated_idx;
}

void Records::Mmap::push_free_slot(int32_t slotidx)
{
    uint64_t id_hash = calculate_id_hash(data_records[slotidx].id) & (ID_HASH_TABLE_SIZE - 1);
    if (id_hash_table[id_hash].slot_idx == slotidx)
        id_hash_table[id_hash].slot_idx = -1;

    data_records[slotidx].is_used = false;
    std::memset(data_records[slotidx].payload.data(), 0, data_records[slotidx].payload.size());
    data_records[slotidx].ttl = 0;
    data_records[slotidx].priority = 0;
    data_records[slotidx].rdata_len = 0;
    data_records[slotidx].group_id.clock_seq_and_node = 0;
    data_records[slotidx].group_id.time_and_version = 0;
    data_records[slotidx].id.clock_seq_and_node = 0;
    data_records[slotidx].id.time_and_version = 0;
    data_records[slotidx].next_index = free_list_head_idx;
    data_records[slotidx].prev_index = -1;
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
    return static_cast<size_t>(-1);
}
