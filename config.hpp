#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <string>
#include <vector>
#include <exception>

#include "glm.hpp"

#include "scene.hpp"

struct ConfigFileException : public std::runtime_error{
    ConfigFileException(const std::string& what ) : std::runtime_error(what) {}
};

class Config{
public:
    static Config CreateFromFile(std::string path);

    std::string comment;
    std::string model_file;
    std::string output_file;
    unsigned int recursion_level;
    unsigned int xres, yres;
    glm::vec3 view_point;
    glm::vec3 look_at;
    glm::vec3 up_vector;
    float yview;
    std::vector<Light> lights;
    Color sky_color = Color(0.0, 0.0, 0.0);
    unsigned int multisample = 1;
};

#endif // __CONFIG_HPP__
