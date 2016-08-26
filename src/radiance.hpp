#ifndef __RADIANCE_HPP__
#define __RADIANCE_HPP__

#include "glm.hpp"
#include <assimp/scene.h> // for aiColor3D

struct Color{
    Color() : r(0.0), g(0.0), b(0.0) {}
    Color(float r, float g, float b) : r(r), g(g), b(b) {}
    Color(const aiColor3D& c) : r(c.r), g(c.g), b(c.b) {}
    Color(const glm::vec3& c) : r(c.r), g(c.g), b(c.b) {}
    float r,g,b; // 0 - 1
    Color  operator* (float q)            const {return Color(q*r, q*g, q*b);}
    Color  operator/ (float q)            const {return Color(r/q, g/q, b/q);}
    Color  operator* (const Color& other) const {return Color(other.r*r, other.g*g, other.b*b);}
    Color  operator+ (const Color& o)     const {return Color(r+o.r,g+o.g,b+o.b);}
    Color& operator+=(const Color& o) {*this = *this + o; return *this;}
};

inline Color operator*(float q, const Color& c){
    return Color(q*c.r, q*c.g, q*c.b);
}

struct Radiance{
    Radiance() : r(0.0), g(0.0), b(0.0) {}
    Radiance(float r, float g, float b) : r(r), g(g), b(b) {}
    explicit Radiance(const Color& c){
        r = pow(c.r, 1.0 * 2.2);
        g = pow(c.g, 1.0 * 2.2);
        b = pow(c.b, 1.0 * 2.2);
    }
    float r,g,b; // unbounded, positive
    Radiance  operator* (float q)           const {return Radiance(q*r, q*g, q*b);}
    Radiance  operator* (const Color& c)    const {return Radiance(r*c.r, g*c.g, b*c.b);}
    Radiance  operator* (const Radiance& c) const {return Radiance(r*c.r, g*c.g, b*c.b);}
    Radiance  operator/ (float q)           const {return Radiance(r/q, g/q, b/q);}
    Radiance  operator+ (const Radiance& o) const {return Radiance(r+o.r,g+o.g,b+o.b);}
    Radiance  operator- (const Radiance& o) const {return Radiance(r-o.r,g-o.g,b-o.b);}
    Radiance& operator+=(const Radiance& o) {*this = *this + o; return *this;}
    Radiance& operator-=(const Radiance& o) {*this = *this - o; return *this;}
    Radiance  operator/=(float q)           {*this = *this / q; return *this;}
    Radiance  operator*=(float q)           {*this = *this * q; return *this;}
    Radiance  operator*=(const Radiance& o) {*this = *this * o; return *this;}
};



#endif //__RADIANCE_HPP__
