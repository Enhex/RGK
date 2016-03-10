#ifndef __SCENE_HPP__
#define __SCENE_HPP__

#include <assimp/scene.h>

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <unordered_map>

#include "ray.hpp"

struct Light{
    glm::vec3 pos;
    Color color;
    float intensity;
};


class Scene;

struct Triangle{
    const Scene* parent_scene;
    unsigned int va, vb, vc; // Vertex and normal indices
    unsigned int mat; // Material index
    glm::vec4 p; // plane
    glm::vec3 generic_normal() const {return p.xyz();}
};

struct Material{
    const Scene* parent_scene;
    Color diffuse;
    Color specular;
    float exponent;
};

struct Intersection{
    const Triangle* triangle = nullptr;
    float t;
    // TODO Baricentric coordinates
};

class Scene{
public:
    Scene() {};
    ~Scene();

    void LoadNode(const aiScene* scene, const aiNode* ainode, aiMatrix4x4 current_transform = aiMatrix4x4()) const;
    void LoadMesh(const aiMesh* mesh, aiMatrix4x4 current_transform) const;
    void LoadScene(const aiScene* scene) const;

    // Copies the data from load buffers to optimized, contignous structures.
    void Commit();

    // Prints the entire buffer to stdout.
    void Dump() const;

    Intersection FindIntersect(const Ray& r) const;


    // TODO: have triangle access these, and keep these fields private
    glm::vec3*     vertices = nullptr;
    unsigned int n_vertices = 0;
    Triangle*      triangles = nullptr;
    unsigned int n_triangles = 0;
    Material*      materials = nullptr;
    unsigned int n_materials = 0;
    glm::vec3*     normals = nullptr;
    unsigned int n_normals = 0;

private:


    mutable std::vector<aiVector3D> vertices_buffer;
    mutable std::vector<Triangle> triangles_buffer;
    mutable std::vector<Material> materials_buffer;
    mutable std::vector<aiVector3D> normals_buffer;

    void FreeBuffers();

    bool TestTriangleIntersection(const Triangle& tri,const Ray& r, /*out*/ float& t, bool debug = false) const __attribute__((hot));
    static void CalculateTrianglePlane(Triangle& t) __attribute__((hot));
};

#endif //__SCENE_HPP__
