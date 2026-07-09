#include "Cache/MmapSystem.hpp"

System::Mmap::~Mmap()
{
    if (mmap_base)
        munmap(mmap_base, total_file_size);
}

bool System::Mmap::init(const char *filepath)
{
    total_file_size = MAX_DATA_RECORDS * sizeof(SystemMetadata);

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

    system_metadata = reinterpret_cast<SystemMetadata *>(mmap_base);

    if (is_new_file)
    {
        free_list_head_idx = 0;
        for (size_t i = 0; i < MAX_DATA_RECORDS; ++i)
        {
            CassUuid uuid;
            uuid.clock_seq_and_node = 0;
            uuid.time_and_version = 0;

            system_metadata[i].type = MAX_DATA_RECORDS + 1;
            system_metadata[i].version = uuid;
        }
    }

    return true;
}

bool System::Mmap::get_record(const int type, CassUuid &version)
{
    if (type < 0 || type > MAX_DATA_RECORDS)
        return false;

    if (system_metadata[type].type > MAX_DATA_RECORDS)
        return false;

    version = system_metadata[type].version;
    return true;
}

bool System::Mmap::append_record(const int type, CassUuid version)
{
    system_metadata[type].type = type;
    system_metadata[type].version = version;

    return true;
}

bool System::Mmap::delete_record(const int type)
{
    system_metadata[type].version.clock_seq_and_node = 0;
    system_metadata[type].version.time_and_version = 0;

    system_metadata[type].type = MAX_DATA_RECORDS + 1;

    return true;
}