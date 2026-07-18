#include "Cache/MmapIpGroup.hpp"

IpGroupEntry::Mmap::~Mmap()
{
    if (mmap_base)
        munmap(mmap_base, total_file_size);
}

bool IpGroupEntry::Mmap::init(const char *filepath)
{
    size_t index_zone_size = HASH_TABLE_SIZE * sizeof(IndexBucket);
    size_t id_zone_size = ID_HASH_TABLE_SIZE * sizeof(IDBucket);
    size_t data_zone_size = MAX_DATA_RECORDS * sizeof(IpGroupEntry);

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
    data_entries = reinterpret_cast<IpGroupEntry *>(mmap_base + index_zone_size + id_zone_size);

    if (is_new_file)
    {
        std::memset(hash_table, 0, index_zone_size);
        std::memset(id_hash_table, 0, id_zone_size);

        for (size_t i = 0; i < MAX_DATA_RECORDS; ++i)
        {
            // ID
            data_entries[i].id.clock_seq_and_node = 0;
            data_entries[i].id.time_and_version = 0;

            // GROUP ID
            data_entries[i].group_id.clock_seq_and_node = 0;
            data_entries[i].group_id.time_and_version = 0;

            data_entries[i].priority = 0;
            data_entries[i].is_used = false;

            data_entries[i].next_index = -1;
            data_entries[i].prev_index = -1;

            std::memset(data_entries[i].country_code, 0, 7);
            std::memset(data_entries[i].ip, 0, 12);
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
            if (data_entries[i].is_used)
            {
                uint64_t id_hash = calculate_hash_from_uuid(data_entries[i].id) & (ID_HASH_TABLE_SIZE - 1);
                id_hash_table[id_hash].slot_idx = i;
            }
        }
    }

    free_list_head_idx = -1;
    for (int32_t i = static_cast<int32_t>(MAX_DATA_RECORDS) - 1; i >= 0; --i)
    {
        if (!data_entries[i].is_used)
        {
            data_entries[i].next_index = free_list_head_idx;
            free_list_head_idx = i;
        }
    }

    return true;
}

bool IpGroupEntry::Mmap::get_record(const CassUuid group_id, char countryCode[7], std::vector<IpGroupEntryResponse> &out_entries)
{
    out_entries.clear();
    uint64_t hash = calculate_hash_from_uuid(group_id);
    size_t bucket_idx = find_bucket(hash, countryCode, group_id);

    if (bucket_idx == static_cast<size_t>(-1) || hash_table[bucket_idx].group_id_hash == 0)
    {
        return false;
    }

    int32_t current_slot = hash_table[bucket_idx].head_slot_idx;
    size_t visited = 0;
    while (current_slot >= 0 && current_slot < static_cast<int32_t>(MAX_DATA_RECORDS))
    {
        if (!data_entries[current_slot].is_used)
            break;

        if (++visited > MAX_DATA_RECORDS)
            break;

        IpGroupEntryResponse node;
        node.ip.assign(data_entries[current_slot].ip, data_entries[current_slot].ip + data_entries[current_slot].len);
        node.priority = data_entries[current_slot].priority;
        node.id = data_entries[current_slot].id;

        out_entries.push_back(node);

        int32_t next_slot = data_entries[current_slot].next_index;
        if (next_slot == current_slot)
            break;

        current_slot = next_slot;
    }
    return !out_entries.empty();
}

bool IpGroupEntry::Mmap::append_record(CassUuid groupId, CassUuid id, char countryCode[7], std::vector<uint8_t> val, int priority)
{
    uint64_t hash = calculate_hash_from_uuid(groupId);
    size_t bucket_idx = find_bucket(hash, countryCode, groupId);

    if (bucket_idx == static_cast<size_t>(-1))
        return false;

    uint64_t idHash = calculate_hash_from_uuid(id) & (ID_HASH_TABLE_SIZE - 1);

    if (id_hash_table[idHash].slot_idx != -1)
        return false;

    int32_t new_slot_idx = pop_free_slot();
    if (new_slot_idx == -1)
        return false;

    id_hash_table[idHash].slot_idx = new_slot_idx;

    data_entries[new_slot_idx].group_id = groupId;
    data_entries[new_slot_idx].id = id;
    memcpy(data_entries[new_slot_idx].ip, val.data(), val.size());
    data_entries[new_slot_idx].len = val.size();
    data_entries[new_slot_idx].priority = priority;
    memcpy(data_entries[new_slot_idx].country_code, countryCode, 7);
    data_entries[new_slot_idx].is_used = true;

    if (hash_table[bucket_idx].group_id_hash == 0)
    {
        hash_table[bucket_idx].group_id_hash = hash;
        memcpy(hash_table[bucket_idx].country_code, data_entries[new_slot_idx].country_code, 7);
        hash_table[bucket_idx].head_slot_idx = new_slot_idx;
        data_entries[new_slot_idx].next_index = -1;
        data_entries[new_slot_idx].prev_index = -1;
    }
    else
    {
        int32_t old_head = hash_table[bucket_idx].head_slot_idx;

        if (old_head == new_slot_idx || !data_entries[old_head].is_used)
        {
            int32_t salvage_head = -1;
            if (old_head != new_slot_idx && old_head >= 0 &&
                old_head < static_cast<int32_t>(MAX_DATA_RECORDS))
            {
                int32_t salvage = data_entries[old_head].next_index;
                size_t salvage_visited = 0;
                while (salvage >= 0 && salvage < static_cast<int32_t>(MAX_DATA_RECORDS))
                {
                    if (++salvage_visited > MAX_DATA_RECORDS)
                        break;
                    if (data_entries[salvage].is_used)
                    {
                        salvage_head = salvage;
                        break;
                    }
                    salvage = data_entries[salvage].next_index;
                }
            }

            hash_table[bucket_idx].head_slot_idx = new_slot_idx;
            data_entries[new_slot_idx].next_index = salvage_head;
            data_entries[new_slot_idx].prev_index = -1;
            if (salvage_head != -1)
                data_entries[salvage_head].prev_index = new_slot_idx;
        }
        else
        {
            data_entries[old_head].prev_index = new_slot_idx;
            data_entries[new_slot_idx].next_index = old_head;
            data_entries[new_slot_idx].prev_index = -1;
            hash_table[bucket_idx].head_slot_idx = new_slot_idx;
        }
    }
    return true;
}

bool IpGroupEntry::Mmap::update_record(CassUuid groupId, CassUuid id, char countryCode[7], std::vector<uint8_t> val, int priority)
{
    uint64_t id_hash = calculate_hash_from_uuid(id) & (ID_HASH_TABLE_SIZE - 1);

    if (id_hash_table[id_hash].slot_idx == -1)
        return false;

    int32_t slot_idx = id_hash_table[id_hash].slot_idx;

    if (!data_entries[slot_idx].is_used ||
        data_entries[slot_idx].id.clock_seq_and_node != id.clock_seq_and_node ||
        data_entries[slot_idx].id.time_and_version != id.time_and_version ||
        data_entries[slot_idx].group_id.clock_seq_and_node != groupId.clock_seq_and_node ||
        data_entries[slot_idx].group_id.time_and_version != groupId.time_and_version)
    {
        return false;
    }

    std::memset(data_entries[slot_idx].ip, 0, sizeof(data_entries[slot_idx].ip));
    memcpy(data_entries[slot_idx].ip, val.data(), val.size());
    data_entries[slot_idx].len = val.size();
    data_entries[slot_idx].priority = priority;
    memcpy(data_entries[slot_idx].country_code, countryCode, 7);

    return true;
}

bool IpGroupEntry::Mmap::delete_record(const CassUuid group_id, char country_code[7], int priority)
{
    uint64_t hash = calculate_hash_from_uuid(group_id);
    size_t bucket_idx = find_bucket(hash, country_code, group_id);

    if (bucket_idx == static_cast<size_t>(-1) || hash_table[bucket_idx].group_id_hash == 0)
    {
        return false;
    }

    if (data_entries[hash_table[bucket_idx].head_slot_idx].group_id.clock_seq_and_node != group_id.clock_seq_and_node ||
        data_entries[hash_table[bucket_idx].head_slot_idx].group_id.time_and_version != group_id.time_and_version)
        return false;

    int32_t current_slot = hash_table[bucket_idx].head_slot_idx;
    size_t visited = 0;
    while (current_slot >= 0 && current_slot < static_cast<int32_t>(MAX_DATA_RECORDS))
    {
        if (++visited > MAX_DATA_RECORDS)
            break;

        int32_t next = data_entries[current_slot].next_index;

        uint64_t id_hash = calculate_hash_from_uuid(data_entries[current_slot].id) & (ID_HASH_TABLE_SIZE - 1);
        if (id_hash_table[id_hash].slot_idx == current_slot)
            id_hash_table[id_hash].slot_idx = -1;

        push_free_slot(current_slot);
        current_slot = next;
    }

    hash_table[bucket_idx].group_id_hash = 0;
    memset(hash_table[bucket_idx].country_code, 0, 7);
    hash_table[bucket_idx].head_slot_idx = -1;

    return true;
}

bool IpGroupEntry::Mmap::delete_record_from_uuid(CassUuid group_id, CassUuid id)
{
    uint64_t id_hash = calculate_hash_from_uuid(id) & (ID_HASH_TABLE_SIZE - 1);

    if (id_hash_table[id_hash].slot_idx == -1)
        return false;

    int32_t slot_idx = id_hash_table[id_hash].slot_idx;

    if (!data_entries[slot_idx].is_used ||
        data_entries[slot_idx].id.clock_seq_and_node != id.clock_seq_and_node ||
        data_entries[slot_idx].id.time_and_version != id.time_and_version)
    {
        return false;
    }

    uint64_t hash = calculate_hash_from_uuid(group_id);
    size_t bucket_idx = find_bucket(hash, data_entries[slot_idx].country_code, group_id);

    if (bucket_idx == static_cast<size_t>(-1))
        return false;

    if (data_entries[slot_idx].group_id.clock_seq_and_node != group_id.clock_seq_and_node ||
        data_entries[slot_idx].group_id.time_and_version != group_id.time_and_version)
    {
        return false;
    }

    int32_t prev = data_entries[slot_idx].prev_index;
    int32_t next = data_entries[slot_idx].next_index;

    if (slot_idx == hash_table[bucket_idx].head_slot_idx)
    {
        hash_table[bucket_idx].head_slot_idx = next;
    }
    else if (prev != -1)
    {
        data_entries[prev].next_index = next;
    }

    if (next != -1)
    {
        data_entries[next].prev_index = prev;
    }

    if (hash_table[bucket_idx].head_slot_idx == -1)
    {
        hash_table[bucket_idx].group_id_hash = 0;
        memset(hash_table[bucket_idx].country_code, 0, 7);
    }

    push_free_slot(slot_idx);
    id_hash_table[id_hash].slot_idx = -1;

    return true;
}

uint64_t IpGroupEntry::Mmap::calculate_hash_from_uuid(const CassUuid &uuid) const
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

int32_t IpGroupEntry::Mmap::pop_free_slot()
{
    if (free_list_head_idx == -1)
        return -1;

    int32_t allocated_idx = free_list_head_idx;
    free_list_head_idx = data_entries[allocated_idx].next_index;

    data_entries[allocated_idx].is_used = true;
    data_entries[allocated_idx].next_index = -1;
    data_entries[allocated_idx].prev_index = -1;
    return allocated_idx;
}

void IpGroupEntry::Mmap::push_free_slot(int32_t slotidx)
{
    data_entries[slotidx].is_used = false;

    std::memset(data_entries[slotidx].country_code, 0, 7);
    std::memset(data_entries[slotidx].ip, 0, 12);

    data_entries[slotidx].group_id.clock_seq_and_node = 0;
    data_entries[slotidx].group_id.time_and_version = 0;

    data_entries[slotidx].id.clock_seq_and_node = 0;
    data_entries[slotidx].id.time_and_version = 0;

    data_entries[slotidx].priority = 0;
    data_entries[slotidx].next_index = free_list_head_idx;
    data_entries[slotidx].prev_index = -1;
    free_list_head_idx = slotidx;
}

size_t IpGroupEntry::Mmap::find_bucket(uint64_t hash, const char country_code[7], const CassUuid &group_id) const
{
    size_t index = hash & (HASH_TABLE_SIZE - 1);
    size_t start_index = index;

    while (true)
    {
        if (hash_table[index].group_id_hash == 0)
        {
            return index;
        }

        if (hash_table[index].group_id_hash == hash &&
            std::strncmp(hash_table[index].country_code, country_code, 8) == 0)
        {
            return index;
        }

        index = (index + 1) & (HASH_TABLE_SIZE - 1);
        if (index == start_index)
            break;
    }
    return static_cast<size_t>(-1);
}