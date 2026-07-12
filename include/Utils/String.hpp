#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

namespace Utils
{
    class String
    {
    public:
        static std::vector<std::string> splitBySpace(const std::string &str)
        {
            std::vector<std::string> result;
            std::istringstream iss(str);
            std::string token;
            while (iss >> token)
            {
                result.push_back(token);
            }
            return result;
        }

        static bool getParamFromCharBuffer(const char *buffer, const char *key, char *outValue, size_t maxLen)
        {
            size_t keyLen = strlen(key);

            const char *ptr = strstr(buffer, key);

            if (ptr && *(ptr + keyLen) == '=')
            {
                ptr += keyLen + 1;

                size_t i = 0;
                while (*ptr != '\0' && *ptr != '&' && i < maxLen - 1)
                {
                    outValue[i++] = *ptr++;
                }
                outValue[i] = '\0';
                return true;
            }

            return false;
        }
    };
} // namespace Utils