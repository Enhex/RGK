#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

#include <string>
#include <vector>
#include <exception>

#include "glm.hpp"
#include "brdf.hpp"
#include "primitives.hpp"

struct ConfigFileException : public std::runtime_error{
    ConfigFileException(const std::string& what ) : std::runtime_error(what) {}
};

class Config{
public:
    //static Config CreateFromFile(std::string path);

    std::string comment;
    std::string model_file;
    std::string output_file;
    unsigned int recursion_level;
    unsigned int xres, yres;
    glm::vec3 camera_position;
    glm::vec3 camera_lookat;
    glm::vec3 camera_upvector;
    float yview;
    std::vector<Light> lights;
    Color sky_color = Color(0.0, 0.0, 0.0);
    float sky_brightness = 2.0;
    unsigned int multisample = 1;
    float lens_size = 0.0f;
    float focus_plane = 1.0f;
    float bumpmap_scale = 10.0f;
    float clamp = 100000.0f;
    float russian = -1.0f;
    unsigned int rounds = 1;
    bool force_fresnell = false;
    unsigned int reverse = 0;
    std::string brdf = "cooktorr";
    std::vector<std::string> thinglass;
protected:
    Config(){};
};

class ConfigRTC : public Config{
public:
    static std::shared_ptr<ConfigRTC> CreateFromFile(std::string path);
private:
    ConfigRTC(){};
};

class ConfigJSON : public Config{
public:
    static std::shared_ptr<ConfigJSON> CreateFromFile(std::string path);
private:
    ConfigJSON(){};
};

#endif // __CONFIG_HPP__
