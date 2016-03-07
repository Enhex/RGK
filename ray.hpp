#ifndef __RAY_HPP__
#define __RAY_HPP__

#define GLM_FORCE_RADIANS
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

//TODO: Move this to utils
inline std::ostream& operator<<(std::ostream& stream, glm::vec3 v){
    stream << "[" << v.x << ", " << v.y << ", " << v.z << "]";
    return stream;
}
struct Color{
    float r,g,b;
};

class Ray{
public:
    Ray() {}
    Ray(glm::vec3 from, glm::vec3 dir) :
        origin(from){
        direction = glm::normalize(dir);
    }
    glm::vec3 origin;
    glm::vec3 direction;
    float near = 0.0f;
    float far = 100.0f;
    glm::vec3 t(float t) {return origin + t*direction;}
};

#endif // __RAY_HPP__
