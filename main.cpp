#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>

#include "scene.hpp"
#include "ray.hpp"

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

    unsigned int xres = 10, yres = 10;
    float output[xres][yres][3];

    glm::vec3 camerapos(0.0, 0.0, 4.0);
    glm::vec3 cameradir = glm::normalize( -camerapos ); // look at origin
    glm::vec3 cameraup(0.0,1.0,0.0);
    glm::vec3 cameraleft = glm::normalize(glm::cross(cameradir, cameraup));

    glm::vec3 viewplane = camerapos + cameradir - cameraup + cameraleft;
    glm::vec3 viewplane_x = - cameraleft - cameraleft;
    glm::vec3 viewplane_y =   cameraup   + cameraup;

    for(unsigned int x = 0; x < xres; x++){
        for(unsigned int y = 0; y < yres; y++){
            glm::vec3 xo = (1.0f/xres) * (x + 0.5f) * viewplane_x;
            glm::vec3 yo = (1.0f/yres) * (y + 0.5f) * viewplane_y;
            glm::vec3 p = viewplane + xo + yo;
            std::cout << p << std::endl;
            // TODO: cast a ray from cameradir to p
            output[x][y][0] = 1.0;
            output[x][y][1] = 0.5;
            output[x][y][2] = 0.2;
        }
    }

    (void)output;

    return 0;
}
