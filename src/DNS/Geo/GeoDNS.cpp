#include "DNS/Geo/GeoDNS.hpp"

void GeoDNS::test()
{
    const char *db_path = "GeoDNS/GeoLite2-City.mmdb";
    const char *ip = "8.8.8.8";

    MMDB_s mmdb;
    int status = MMDB_open(db_path, MMDB_MODE_MMAP, &mmdb);
    if (status != MMDB_SUCCESS)
    {
        std::cerr << "MMDB open error: " << MMDB_strerror(status) << "\n";
        return;
    }

    int fd = open(db_path, O_RDONLY);
    if (fd >= 0)
    {
        struct stat st{};
        if (fstat(fd, &st) == 0)
        {
            size_t size = st.st_size;
            size_t pagesize = sysconf(_SC_PAGESIZE);
            for (size_t offset = 0; offset < size; offset += pagesize)
            {
                volatile char *buf;
                pread(fd, &buf, 1, offset);
            }
        }
        close(fd);
    }
    
    int gai_error, mmdb_error;
    MMDB_lookup_result_s result = MMDB_lookup_string(&mmdb, ip, &gai_error, &mmdb_error);

    if (gai_error != 0)
    {
        std::cerr << "IP error: " << gai_strerror(gai_error) << "\n";
    }
    else if (mmdb_error != MMDB_SUCCESS)
    {
        std::cerr << "MMDB lookup error: " << MMDB_strerror(mmdb_error) << "\n";
    }
    else if (result.found_entry)
    {
        MMDB_entry_data_s entry_data;
        if (MMDB_get_value(&result.entry, &entry_data, "country", "iso_code", NULL) == MMDB_SUCCESS)
        {
            if (entry_data.has_data)
            {
                std::cout << "Country: " << std::string(entry_data.utf8_string, entry_data.data_size) << "\n";
            }
        }
    }

    MMDB_close(&mmdb);
}
