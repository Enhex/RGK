#ifndef __SCENE_HPP__
#define __SCENE_HPP__

#include <assimp/scene.h>

#include <vector>
#include <set>
#include <unordered_map>

#include "glm.hpp"

#include "ray.hpp"
#include "primitives.hpp"
#include "utils.hpp"
#include "texture.hpp"
#include "config.hpp"
#include "random.hpp"

struct UncompressedKdNode;
struct CompressedKdNode;

class Scene{
public:
    Scene() {};
    ~Scene();

    void LoadNode(const aiScene* scene, const aiNode* ainode, aiMatrix4x4 current_transform = aiMatrix4x4());
    void LoadMesh(const aiMesh* mesh, aiMatrix4x4 current_transform);
    void LoadMaterial(const aiMaterial* mat, const std::shared_ptr<Config> cfg);
    void LoadScene(const aiScene* scene, const std::shared_ptr<Config> cfg);

    // Copies the data from load buffers to optimized, contignous structures.
    void Commit();
    // Compresses the kd-tree. Called automatically by Commit()
    void Compress();

    // Prints the entire buffer to stdout.
    void Dump() const;

    // Searches for the nearest intersection in the diretion specified by ray.
    Intersection    FindIntersectKd   (const Ray& r)
        __restrict__ const __attribute__((hot));
    // Searches for any interection (not necessarily nearest). Slightly faster than FindIntersectKd.
    const Triangle* FindIntersectKdAny(const Ray& r)
        __restrict__ const __attribute__((hot));
    // Searches for the nearest intersection, but ignores all intersections with the specified ignored triangle.
    Intersection    FindIntersectKdOtherThan(const Ray& r, const Triangle* ignored)
        __restrict__ const __attribute__((hot));
    // Searches for the nearest intersection, but ignores both the specified ignored triangle, as well as all triangles
    //  that use material specified in thinglass set. However, such materials are gathered into a list (ordered) in the
    //  returned value. Useful for simulating thin colored glass.
    Intersection    FindIntersectKdOtherThanWithThinglass(const Ray& r, const Triangle* ignored, const std::set<const Material*>& thinglass)
        __restrict__ const __attribute__((hot));

    // Returns true IFF the two points are visible from each other.
    // Incorporates no cache of any kind.
    bool Visibility(glm::vec3 a, glm::vec3 b) __restrict__ const __attribute__((hot));
    bool VisibilityWithThinglass(glm::vec3 a, glm::vec3 b, const std::set<const Material*>& thinglass,
                                 /*out*/ std::vector<std::pair<const Triangle*, float>>&)
        __restrict__ const __attribute__((hot));

    // Makes a set of material references that match any of given substrings.
    std::set<const Material*> MakeMaterialSet(std::vector<std::string>) const;

    // TODO: have triangle access these, and keep these fields private
    glm::vec3*     vertices  = nullptr;
    unsigned int n_vertices  = 0;
    Triangle*      triangles = nullptr;
    unsigned int n_triangles = 0;
    Material*      materials = nullptr;
    unsigned int n_materials = 0;
    glm::vec3*     normals   = nullptr;
    unsigned int n_normals   = 0;
    glm::vec3*     tangents  = nullptr;
    unsigned int n_tangents  = 0;
    glm::vec2*     texcoords = nullptr;
    unsigned int n_texcoords = 0;

    // Point lights
    std::vector<Light> pointlights;
    void AddPointLights(std::vector<Light>);
    // Areal lights
    struct ArealLight{
        // TODO: Rember to sort this (descending order)
        std::vector<std::pair<float,unsigned int>> triangles_with_areas;
        mutable float total_area = 0.0f;

        Color emission;
        float power;

        Light GetRandomLight(Random&, const Scene& parent) const;
    };
    float total_areal_power;
    float total_point_power;
    std::vector<std::pair<float,ArealLight>> areal_lights;


    Light GetRandomLight(Random& rnd) const;


    // Indexed by triangles.
    std::vector<float> xevents;
    std::vector<float> yevents;
    std::vector<float> zevents;

    // The bounding box for the entire scene.
    std::pair<float,float> xBB;
    std::pair<float,float> yBB;
    std::pair<float,float> zBB;

    std::string texture_directory;

    // Dynamically determined by examining scene's diameter
    float epsilon = 0.0001f;

private:

    UncompressedKdNode* uncompressed_root = nullptr;

    CompressedKdNode* compressed_array = nullptr;
    unsigned int compressed_array_size = 0;
    unsigned int* compressed_triangles = nullptr;
    unsigned int compressed_triangles_size = 0;

    void CompressRec(const UncompressedKdNode* node, unsigned int& array_pos, unsigned int& triangle_pos);

    mutable std::vector<aiVector3D> vertices_buffer;
    mutable std::vector<Triangle> triangles_buffer;
    mutable std::vector<Material> materials_buffer;
    mutable std::vector<aiVector3D> normals_buffer;
    mutable std::vector<aiVector3D> tangents_buffer;
    mutable std::vector<aiVector3D> texcoords_buffer;

    std::unordered_map<std::string, Texture*> textures;
    Texture* GetTexture(std::string name);

    void FreeBuffers();
    void FreeTextures();
    void FreeCompressedTree();
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
    std::vector<unsigned int> triangle_indices;

    void Subdivide(unsigned int max_depth);
    UncompressedKdNode* ch0 = nullptr;
    UncompressedKdNode* ch1 = nullptr;

    float prob0, prob1;

    int dups = 0;
    // Total triangles / leaf nodes / total nodes / total dups
    std::tuple<int, int, int, int> GetTotals() const;

    void FreeRecursivelly();

    float GetCost() const;

    int split_axis;
    float split_pos;
};

struct CompressedKdNode{
    inline bool IsLeaf() const {return (kind & 0x03) == 0x03;}
    inline short GetSplitAxis() const {return kind & 0x03;}
    inline float GetSplitPlane() const {return split_plane;}
    inline uint32_t GetTrianglesN() const {return triangles_num >> 2;}
    inline uint32_t GetFirstTrianglePos() const {return triangles_start;}
    inline uint32_t GetOtherChildIndex() const {return other_child >> 2;}

    // Default constructor;
    CompressedKdNode() {}

    // Constructor for internal nodes
    CompressedKdNode(short axis, float split) {
        kind = axis;
        split_plane = split;
    }
    // Constructor for leaf nodes
    CompressedKdNode(unsigned int num, unsigned int start){
        triangles_num = (num << 2) | 0x03;
        triangles_start = start;
    }
    // Once the other child is placed, it's position has to be set in parent node
    inline void SetOtherChild(unsigned int pos){
        other_child = (other_child & 0x03) | (pos << 2);
    }

private:
    union{
        float split_plane; // For internal nodes
        uint32_t triangles_start; // For leaf nodes
    };
    union{
         // For internal nodes (shifted right 2 bits). One child is
         // just after this stuct in memory layout, other is at this
         // location.
        uint32_t other_child;
         // For leaf nodes (shifter right 2 bits).
        uint32_t triangles_num;
        // For any kind of node, 2 LSB.
        uint32_t kind;
    };
} __attribute__((packed,aligned(4)));

#endif //__SCENE_HPP__
