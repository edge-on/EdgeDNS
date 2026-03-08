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
    };
} // namespace Utils