#ifndef __PRIMITITES_HPP__
#define __PRIMITITES_HPP__

#include <assimp/scene.h>

#include "glm.hpp"

#include "ray.hpp"

class Scene;

struct Color{
    Color() : r(0.0), g(0.0), b(0.0) {}
    Color(float r, float g, float b) : r(r), g(g), b(b) {}
    Color(const aiColor3D& c) : r(c.r), g(c.g), b(c.b) {}
    float r,g,b; // 0 - 1
    Color  operator* (float q)            const {return Color(q*r, q*g, q*b);}
    Color  operator* (const Color& other) const {return Color(other.r*r, other.g*g, other.b*b);}
    Color  operator+ (const Color& o)     const {return Color(r+o.r,g+o.g,b+o.b);}
    Color& operator+=(const Color& o) {*this = *this + o; return *this;}
};

inline Color operator*(float q, const Color& c){
    return Color(q*c.r, q*c.g, q*c.b);
}

struct Light{
    glm::vec3 pos;
    Color color;
    float intensity;
};

struct Material{
    const Scene* parent_scene;
    Color diffuse;
    Color specular;
    float exponent;
};

class Triangle{
public:
    const Scene* parent_scene;
    unsigned int va, vb, vc; // Vertex and normal indices
    unsigned int mat; // Material index
    glm::vec4 p; // plane
    glm::vec3 generic_normal() const {return p.xyz();}

    const Material& GetMaterial() const;
    const glm::vec3 GetVertexA()  const;
    const glm::vec3 GetVertexB()  const;
    const glm::vec3 GetVertexC()  const;
    const glm::vec3 GetNormalA()  const;
    const glm::vec3 GetNormalB()  const;
    const glm::vec3 GetNormalC()  const;
};

struct Intersection{
    const Triangle* triangle = nullptr;
    float t;
    float a,b,c;
    template <typename T>
    T Interpolate(const T& x, const T& y, const T& z) {return a*x + b*y + c*z;}
};


#endif // __PRIMITITES_HPP__
