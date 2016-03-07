#ifndef __SCENE_HPP__
#define __SCENE_HPP__

#include <assimp/scene.h>

#include <vector>
#include <unordered_map>

class Scene;

struct Triangle{
    const Scene* parent_scene;
    unsigned int va, vb, vc; // Vertex indices
    unsigned int mat; // Material index
};

struct Material{
    const Scene* parent_scene;
    aiColor3D diffuse;
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

    aiVector3D*    vertices = nullptr;
    unsigned int n_vertices = 0;
    Triangle*      triangles = nullptr;
    unsigned int n_triangles = 0;
    Material*      materials = nullptr;
    unsigned int n_materials = 0;

private:
    mutable std::vector<aiVector3D> vertices_buffer;
    mutable std::vector<Triangle> triangles_buffer;
    mutable std::vector<Material> materials_buffer;

    void FreeBuffers();
};

#endif //__SCENE_HPP__
