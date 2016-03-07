#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>

#include "scene.hpp"
#include "ray.hpp"

#include <png++/png.hpp>

void write_output_file(std::string path, std::vector<std::vector<Color>> data, unsigned int xsize, unsigned int ysize){
    png::image<png::rgb_pixel> image(xsize, ysize);

    for (png::uint_32 y = 0; y < image.get_height(); ++y){
        for (png::uint_32 x = 0; x < image.get_width(); ++x){
            auto c = data[x][y];
            auto px = png::rgb_pixel(255.0*c.r,255.0*c.g,255.0*c.b);
            image[y][x] = px;
            // non-checking equivalent of image.set_pixel(x, y, ...);
        }
    }
    image.write(path);
}

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

    unsigned int xres = 200, yres = 200;
    std::vector<std::vector<Color>> output(xres, std::vector<Color>(yres));

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
            (void)p;
            // TODO: cast a ray from cameradir to p
            Ray r(camerapos, p - camerapos);
            Intersection i = s.FindIntersect(r);

            //std::cout << "Casting a ray from " << camerapos << " to " << r.direction << std::endl;

            if(i.triangle){
                const Material& mat = i.triangle->parent_scene->materials[i.triangle->mat];
                //std::cout << "hit " << mat.diffuse << std::endl;
                output[x][y] = mat.diffuse;
            }else{
                //std::cout << "no hit" << std::endl;
                // Black background for void spaces
                output[x][y] = Color{0.0,0.0,0.0};
            }
        }
    }

    write_output_file("out.png",output, xres, yres);

    return 0;
}
