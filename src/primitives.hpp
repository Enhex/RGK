#ifndef __PRIMITITES_HPP__
#define __PRIMITITES_HPP__

#include <assimp/scene.h>

#include "glm.hpp"

#include "ray.hpp"

class Scene;
class Texture;

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

struct Radiance{
    Radiance() : r(0.0), g(0.0), b(0.0) {}
    Radiance(float r, float g, float b) : r(r), g(g), b(b) {}
    explicit Radiance(const Color& c): r(c.r), g(c.g), b(c.b) {}
    float r,g,b; // unbounded, positive
    Radiance  operator+ (const Radiance& o) const {return Radiance(r+o.r,g+o.g,b+o.b);}
    Radiance& operator+=(const Radiance& o) {*this = *this + o; return *this;}
};

struct Light{
    glm::vec3 pos;
    Color color;
    float intensity;
};

struct Material{
    std::string name;
    const Scene* parent_scene;
    Color diffuse;
    Color specular;
    Color ambient;
    float exponent;
    Texture* diffuse_texture  = nullptr;
    Texture* specular_texture = nullptr;
    Texture* ambient_texture  = nullptr;
    Texture* bump_texture  = nullptr;
};

class Triangle{
public:
    const Scene* parent_scene;
    unsigned int va, vb, vc; // Vertex and normal indices
    unsigned int mat; // Material index
    glm::vec4 p; // plane
    inline glm::vec3 generic_normal() const {return p.xyz();}
    void CalculatePlane() __attribute__((hot));

    Triangle(const Scene* parent, unsigned int va, unsigned int vb, unsigned int vc, unsigned int mat) :
        parent_scene(parent), va(va), vb(vb), vc(vc), mat(mat) {}
    Triangle() : parent_scene(nullptr) {}

    const Material& GetMaterial() const;
    const glm::vec3 GetVertexA()  const;
    const glm::vec3 GetVertexB()  const;
    const glm::vec3 GetVertexC()  const;
    const glm::vec3 GetNormalA()  const;
    const glm::vec3 GetNormalB()  const;
    const glm::vec3 GetNormalC()  const;
    const glm::vec2 GetTexCoordsA()  const;
    const glm::vec2 GetTexCoordsB()  const;
    const glm::vec2 GetTexCoordsC()  const;
    const glm::vec3 GetTangentA()  const;
    const glm::vec3 GetTangentB()  const;
    const glm::vec3 GetTangentC()  const;

    bool TestIntersection(const Ray& r, /*out*/ float& t, float& a, float& b, bool debug = false) const __attribute__((hot));
};

struct Intersection{
    const Triangle* triangle = nullptr;
    float t;
    float a,b,c;
    template <typename T>
    T Interpolate(const T& x, const T& y, const T& z) {return a*x + b*y + c*z;}
};


#endif // __PRIMITITES_HPP__
