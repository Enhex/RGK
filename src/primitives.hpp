#ifndef __PRIMITITES_HPP__
#define __PRIMITITES_HPP__

#include <functional>

#include "glm.hpp"

#include "radiance.hpp"
#include "ray.hpp"
#include "brdf.hpp"

class Scene;
class Texture;

struct Light{
    glm::vec3 pos;
    Color color;
    float intensity;
    float size;
};

struct Material{
    Material(){
        brdf = BRDF::Diffuse;
    }
    std::string name;
    const Scene* parent_scene;
    Color diffuse;
    Color specular;
    Color ambient;
    float exponent;
    float refraction_index;
    Texture* diffuse_texture  = nullptr;
    Texture* specular_texture = nullptr;
    Texture* ambient_texture  = nullptr;
    Texture* bump_texture  = nullptr;
    BRDF_fptr brdf;
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
