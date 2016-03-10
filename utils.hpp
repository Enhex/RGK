#ifndef __UTILS_HPP__
#define __UTILS_HPP__

#include <string>
#include <vector>

class Utils{
public:
    static std::string Trim(std::string);
    static std::vector<std::string> SplitString(std::string str, std::string delimiter);
    static std::string GetDir(std::string path);
    static std::string GetFilename(std::string path);
    static bool GetFileExists(std::string path);
};

#endif // __UTILS_HPP__
