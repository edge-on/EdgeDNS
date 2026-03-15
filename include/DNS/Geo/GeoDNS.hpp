#pragma once

#include <maxminddb.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string>

class GeoDNS
{
public:
    void init();
    std::string getCountry(char *ip);

private:
    MMDB_s mmdb;
    
    const char *db_path = "GeoDNS/GeoLite2-City.mmdb";
};