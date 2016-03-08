#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>
#include <queue>

#include "external/ctpl_stl.h"

#include "scene.hpp"
#include "ray.hpp"
#include "outbuffer.hpp"

typedef glm::vec3 Light;

Color trace_ray(const Scene& scene, const Ray& r, std::vector<Light> lights){
    Intersection i = scene.FindIntersect(r);

    if(i.triangle){
        const Material& mat = i.triangle->parent_scene->materials[i.triangle->mat];
        Color diffuse = mat.diffuse;
        Color total(0.0, 0.0, 0.0);

        glm::vec3 ipos = r[i.t];

        for(const Light& l : lights){
            glm::vec3 vec_to_light = glm::normalize(l - ipos);
            // if no intersection on path to light
            Ray ray_to_light(ipos, l, 0.01);
            Intersection i2 = scene.FindIntersect(ray_to_light);
            if(!i2.triangle){ // no intersection found
                //TODO: use actual normals
                float q = glm::dot(i.triangle->normal(), vec_to_light);
                q = glm::abs(q); // This way we ignore face orientation.
                total += diffuse*q;
            }
        }

        return total;
    }else{
        // Black background for void spaces
        return Color{0.0,0.0,0.0};
    }
}

struct CameraConfig{
public:
    CameraConfig(glm::vec3 pos, glm::vec3 la, glm::vec3 up){
        camerapos = pos;
        lookat = la;
        cameraup = up;

        cameradir = glm::normalize( lookat - camerapos);
        cameraleft = glm::normalize(glm::cross(cameradir, cameraup));
        cameraup = glm::normalize(glm::cross(cameradir,cameraleft));

        viewscreen = camerapos + cameradir - cameraup + cameraleft;
        viewscreen_x = -2.0f * cameraleft;
        viewscreen_y =  2.0f * cameraup;
    }
    glm::vec3 camerapos;
    glm::vec3 lookat;
    glm::vec3 cameraup;
    glm::vec3 cameradir;
    glm::vec3 cameraleft;

    glm::vec3 viewscreen;
    glm::vec3 viewscreen_x;
    glm::vec3 viewscreen_y;

    glm::vec3 GetViewScreenPoint(int x, int y, int xres, int yres) const {
        const float off = 0.5f;
        glm::vec3 xo = (1.0f/xres) * (x + off) * viewscreen_x;
        glm::vec3 yo = (1.0f/yres) * (y + off) * viewscreen_y;
        return viewscreen + xo + yo;
    };
};

struct RenderTask{
    unsigned int xres, yres;
    unsigned int xrange_start, xrange_end;
    unsigned int yrange_start, yrange_end;
    const Scene* scene;
    const CameraConfig* cconfig;
    const std::vector<Light>* lights;
    OutBuffer* output;
};

void Render(RenderTask task){
    for(unsigned int y = task.yrange_start; y < task.yrange_end; y++){
        for(unsigned int x = task.xrange_start; x < task.xrange_end; x++){
            glm::vec3 p = task.cconfig->GetViewScreenPoint(x, y, task.xres, task.yres);
            Ray r(task.cconfig->camerapos, p - task.cconfig->camerapos);
            Color c = trace_ray(*task.scene, r, *task.lights);
            task.output->SetPixel(x, y, c);
        }
    }
}

int main(){

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile("../obj/cube3.obj",
                                             aiProcessPreset_TargetRealtime_MaxQuality
                      );

    if(!scene){
        std::cerr << "Assimp failed to load scene. " << std::endl;
        return 1;
    }

    std::cout << "Loaded scene with " << scene->mNumMeshes << " meshes, " <<
        scene->mNumMaterials << " materials and " << scene->mNumLights <<
        " lights." << std::endl;

    Scene s;
    s.LoadScene(scene);
    s.Commit();

    unsigned int xres = 1000, yres = 1000;

    glm::vec3 camerapos(3.5, 4.0, -2.0);
    glm::vec3 lookat(0.0, 1.0, 0.0);
    glm::vec3 worldup(0.0, 1.0, 0.0);

    CameraConfig cconfig(camerapos, lookat, worldup);

    Light light(4.0,7.0,3.0);
    std::vector<Light> lights = {light};

    OutBuffer ob(xres, yres);

    std::queue<RenderTask> task_queue;

    ctpl::thread_pool tpool(4);

    // Split rendering into smaller (100x100) tasks.
    for(unsigned int yp = 0; yp < yres; yp += 100){
        for(unsigned int xp = 0; xp < xres; xp += 100){
            RenderTask task;
            task.xres = xres; task.yres = yres;
            task.xrange_start = xp; task.xrange_end = std::min(xres,xp+100);
            task.yrange_start = yp; task.yrange_end = std::min(yres,yp+100);
            task.scene = &s;
            task.cconfig = &cconfig;
            task.lights = &lights;
            task.output = &ob;
            tpool.push( [task](int){Render(task);} );
        }
    }

    tpool.stop(true); // Waits for all remaining threads to complete.

    ob.WriteToPNG("out.png");

    return 0;
}
