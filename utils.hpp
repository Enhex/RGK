#ifndef __UTILS_HPP__
#define __UTILS_HPP__

#include <string>
#include <vector>
#include <iostream>

#include "glm.hpp"

#include "primitives.hpp"

inline std::ostream& operator<<(std::ostream& stream, const glm::vec3& v){
    stream << "[" << v.x << ", " << v.y << ", " << v.z << "]";
    return stream;
}
inline std::ostream& operator<<(std::ostream& stream, const Color& c){
    stream << "{" << c.r << ", " << c.g << ", " << c.b << "}";
    return stream;
}

class Utils{
public:
    static std::string Trim(std::string);
    static std::vector<std::string> SplitString(std::string str, std::string delimiter, bool skip_empty = true);
    static std::string GetDir(std::string path);
    static std::string GetFilename(std::string path);
    static bool GetFileExists(std::string path);
};

#endif // __UTILS_HPP__
