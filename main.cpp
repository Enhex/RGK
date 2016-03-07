#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define GLM_FORCE_RADIANS
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

#include "scene.hpp"

int main(){

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile("../obj/cube.obj",
                                             //aiProcess_Triangulate |
                                             //aiProcess_JoinIdenticalVertices
                                             aiProcessPreset_TargetRealtime_MaxQuality
                      );

    if(!scene){
        std::cerr << "Failed to load scene. " << std::endl;
        return 1;
    }

    std::cout << "Loaded scene with " << scene->mNumMeshes << " meshes, " <<
        scene->mNumMaterials << " materials and " << scene->mNumLights <<
        " lights." << std::endl;


    Scene s;
    s.LoadScene(scene);

    s.Commit();

    s.Dump();

    return 0;
}
