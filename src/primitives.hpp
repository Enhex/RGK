#ifndef __PRIMITITES_HPP__
#define __PRIMITITES_HPP__

#include <functional>

#include "glm.hpp"

#include "radiance.hpp"
#include "ray.hpp"
#include "brdf.hpp"
#include "random.hpp"
#include <memory>

// I have to be c++11 compatible! :(
namespace std{
    template <class T, class... Args>
    std::unique_ptr<T> make_unique(Args... a){
        return std::unique_ptr<T>(new T(a...));
    }
}

class Scene;
class Texture;

struct Light{
    enum Type{
        FULL_SPHERE,
        HEMISPHERE,
    };
    Light() {}
    Type type;
    glm::vec3 pos;
    Color color;
    float intensity;
    // TODO: union?
    float size; // Only for full_sphere lights
    glm::vec3 normal; // Only for hemisphere lights
};

struct Material{
    Material(){
        brdf = std::make_unique<BRDFDiffuseCosine>();
    }
    std::string name;
    const Scene* parent_scene;
    Color diffuse;
    Color specular;
    Color ambient;
    Color emission;
    float exponent;
    float refraction_index;
    bool reflective = false;
    bool emissive = false;
    float reflection_strength = 0.0f;
    float translucency = 0.0f;
    Texture* diffuse_texture  = nullptr;
    Texture* specular_texture = nullptr;
    Texture* ambient_texture  = nullptr;
    Texture* bump_texture  = nullptr;
    std::unique_ptr<BRDF> brdf;
};

class Triangle{
public:
    const Scene* parent_scene;
    unsigned int va, vb, vc; // Vertex and normal indices
    unsigned int mat; // Material index
    glm::vec4 p; // plane
    inline glm::vec3 generic_normal() const {return p.xyz();}
    void CalculatePlane() __attribute__((hot));
    float GetArea() const;
    glm::vec3 GetRandomPoint(Random& rnd) const;

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

typedef std::vector<std::pair<const Triangle*,float>> ThinglassIsections;
struct Intersection{
    const Triangle* triangle = nullptr;
    float t;
    float a,b,c;
    template <typename T>
    T Interpolate(const T& x, const T& y, const T& z) {return a*x + b*y + c*z;}
    // An ordered list of intersections with materials that are considered to be a thin glass.
    // The first element of pair is the triangle intersecting. The second is the distance from ray origin
    // to the intersection. The second parameter is used because triangles may get cloned during kD-tree
    // construction, and we need to apply a filter just once.
    ThinglassIsections thinglass;
};


#endif // __PRIMITITES_HPP__
