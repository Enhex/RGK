#ifndef __SCENE_HPP__
#define __SCENE_HPP__

#include <assimp/scene.h>

#include <vector>
#include <unordered_map>

#include "glm.hpp"

#include "ray.hpp"
#include "primitives.hpp"
#include "utils.hpp"

struct UncompressedKdNode;

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
    Intersection FindIntersectKd(const Ray& r, bool debug = false) const;


    // TODO: have triangle access these, and keep these fields private
    glm::vec3*     vertices = nullptr;
    unsigned int n_vertices = 0;
    Triangle*      triangles = nullptr;
    unsigned int n_triangles = 0;
    Material*      materials = nullptr;
    unsigned int n_materials = 0;
    glm::vec3*     normals = nullptr;
    unsigned int n_normals = 0;

    // Indexed by triangles.
    std::vector<float> xevents;
    std::vector<float> yevents;
    std::vector<float> zevents;

    // The bounding box for the entire scene.
    std::pair<float,float> xBB;
    std::pair<float,float> yBB;
    std::pair<float,float> zBB;

private:

    UncompressedKdNode* root = nullptr;

    mutable std::vector<aiVector3D> vertices_buffer;
    mutable std::vector<Triangle> triangles_buffer;
    mutable std::vector<Material> materials_buffer;
    mutable std::vector<aiVector3D> normals_buffer;

    void FreeBuffers();

    bool TestTriangleIntersection(const Triangle& tri,const Ray& r, /*out*/ float& t, float& a, float& b, bool debug = false) const __attribute__((hot));
    static void CalculateTrianglePlane(Triangle& t) __attribute__((hot));
};


#define EMPTY_BONUS 0.5f
#define ISECT_COST 80.0f
#define TRAV_COST 2.0f

struct UncompressedKdNode{
    const Scene* parent_scene;
    enum {LEAF, INTERNAL} type = LEAF;
    unsigned int depth = 0;
    std::pair<float,float> xBB;
    std::pair<float,float> yBB;
    std::pair<float,float> zBB;
    std::vector<int> triangle_indices;

    void Subdivide(unsigned int max_depth);
    UncompressedKdNode* ch0 = nullptr;
    UncompressedKdNode* ch1 = nullptr;

    float prob0, prob1;

    int dups = 0;
    // Total triangles / leaf nodes / total nodes / total dups
    std::tuple<int, int, int, int> GetTotals() const;

    void FreeRecursivelly();

    float GetCost() const;

    float split_axis;
    float split_pos;
};

#endif //__SCENE_HPP__
