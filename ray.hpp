#ifndef __RAY_HPP__
#define __RAY_HPP__

#define GLM_FORCE_RADIANS
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

//TODO: Move this to utils
std::ostream& operator<<(std::ostream& stream, glm::vec3 v){
    stream << "[" << v.x << ", " << v.y << ", " << v.z << "]";
    return stream;
}

class Ray{
public:
    Ray() {}
    glm::vec3 origin;
    glm::vec3 direction;
    float near = 0.0f;
    float far = 100.0f;
};

#endif // __RAY_HPP__
