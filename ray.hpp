#ifndef __RAY_HPP__
#define __RAY_HPP__

#include <assimp/scene.h>

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

struct Color{
    Color() : r(0.0), g(0.0), b(0.0) {}
    Color(float r, float g, float b) : r(r), g(g), b(b) {}
    Color(const aiColor3D& c) : r(c.r), g(c.g), b(c.b) {}
    float r,g,b; // 0 - 255
    Color operator*(float q) const {return Color(q*r, q*g, q*b);}
    Color operator*(const Color& other) const {return Color(other.r*r/255.0f, other.g*g/255.0, other.b*b/255.0);}
    Color operator+(const Color& o) const {return Color(r+o.r,g+o.g,b+o.b);}
    Color& operator+=(const Color& o) {*this = *this + o; return *this;}
};

inline Color operator*(float q, const Color& c){
    return Color(q*c.r, q*c.g, q*c.b);
}

//TODO: Move this to utils
inline std::ostream& operator<<(std::ostream& stream, const glm::vec3& v){
    stream << "[" << v.x << ", " << v.y << ", " << v.z << "]";
    return stream;
}
inline std::ostream& operator<<(std::ostream& stream, const Color& c){
    stream << "{" << c.r << ", " << c.g << ", " << c.b << "}";
    return stream;
}

class Ray{
public:
    Ray() {}
    // Directional constructor (half-line).
    Ray(glm::vec3 from, glm::vec3 dir) :
        origin(from){
        direction = glm::normalize(dir);
    }
    // From-to constructor. Sets near and far.
    Ray(glm::vec3 from, glm::vec3 to, float epsillon){
        origin = from;
        glm::vec3 diff = to-from;
        direction = glm::normalize(diff);
        float length = glm::length(diff);
        near =   0.0f + epsillon;
        far  = length - epsillon;
    }
    glm::vec3 origin;
    glm::vec3 direction;
    float near = 0.0f;
    float far = 100.0f;
    glm::vec3 t(float t) const {return origin + t*direction;}
    inline glm::vec3 operator[](float t_) const {return t(t_);}
};

#endif // __RAY_HPP__
