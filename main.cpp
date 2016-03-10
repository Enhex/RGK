#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>
#include <queue>
#include <cmath>

#include "external/ctpl_stl.h"

#include "scene.hpp"
#include "ray.hpp"
#include "outbuffer.hpp"
#include "config.hpp"
#include "utils.hpp"

Color trace_ray(const Scene& scene, const Ray& r, const std::vector<Light>& lights, int depth){
    Intersection i = scene.FindIntersect(r);

    if(i.triangle){
        const Material& mat = i.triangle->parent_scene->materials[i.triangle->mat];
        Color total(0.0, 0.0, 0.0);

        glm::vec3 ipos = r[i.t];
        glm::vec3 N = glm::normalize(i.triangle->normal());
        glm::vec3 V = glm::normalize(-r.direction); // incoming direction

        for(const Light& l : lights){
            glm::vec3 L = glm::normalize(l.pos - ipos);
            bool shadow;
            if(depth == 0) shadow = false;
            else{
                // if no intersection on path to light
                Ray ray_to_light(ipos, l.pos, 0.01);
                Intersection i2 = scene.FindIntersect(ray_to_light);
                shadow = i2.triangle;
            }
            if(!shadow){ // no intersection found
                //TODO: use actual normals

                float distance = glm::length(ipos - l.pos); // The distance to light
                float d = distance/l.intensity;
                float intens_factor = 1.0f/(1.0f + d); // Light intensity falloff function


                float kD = -glm::dot(N, L);
                kD = glm::max(0.0f, kD);
                total += intens_factor * l.color * mat.diffuse  * kD       ;

                if(mat.exponent > 1.0f){
                    glm::vec3 R = 2.0f * glm::dot(L, N) * N - L;
                    float a = glm::dot(R, V);
                    a = glm::max(0.0f, a);
                    float kS = glm::pow(a, mat.exponent);
                    // if(std::isnan(kS)) std::cout << glm::dot(R,V) << "/" << mat.exponent << std::endl;
                    total += intens_factor * l.color * mat.specular * kS * 1.0f;
                }
            }
        }

        // Next ray
        if(depth >= 2 && mat.exponent <= 1.0f){
            glm::vec3 refl = 2.0f * glm::dot(V, N) * N - V;
            refl = N;
            Ray refl_ray(ipos, ipos + refl, 0.01);
            refl_ray.far = 1000.0f;
            Color reflection = trace_ray(scene, refl_ray, lights, depth-1);
            total = mat.exponent * reflection + (1.0f - mat.exponent) * total;
        }

        return total;
    }else{
        // Black background for void spaces
        return Color{0.0,0.0,0.0};
    }
}

struct CameraConfig{
public:
    CameraConfig(glm::vec3 pos, glm::vec3 la, glm::vec3 up, float yview, float xview){
        camerapos = pos;
        lookat = la;
        cameraup = up;

        cameradir = glm::normalize( lookat - camerapos);
        cameraleft = glm::normalize(glm::cross(cameradir, cameraup));
        cameraup = glm::normalize(glm::cross(cameradir,cameraleft));

        viewscreen_x = - xview * cameraleft;
        viewscreen_y =   yview * cameraup;
        viewscreen = camerapos + cameradir - 0.5f * viewscreen_y - 0.5f * viewscreen_x;
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
    unsigned int multisample;
    unsigned int recursion_level;
    OutBuffer* output;
};

void Render(RenderTask task){
    unsigned int m = task.multisample;
    for(unsigned int y = task.yrange_start; y < task.yrange_end; y++){
        for(unsigned int x = task.xrange_start; x < task.xrange_end; x++){
            Color pixel_total(0.0, 0.0, 0.0);
            float factor = 1.0/(m*m);
            for(unsigned int my = 0; my < m; my++){
                for(unsigned int mx = 0; mx < m; mx++){
                    glm::vec3 p = task.cconfig->GetViewScreenPoint(x*m + mx, y*m + my, task.xres*m, task.yres*m);
                    Ray r(task.cconfig->camerapos, p - task.cconfig->camerapos);
                    pixel_total += trace_ray(*task.scene, r, *task.lights, task.recursion_level) * factor;
                }
            }
            task.output->SetPixel(x, y, pixel_total);
        }
    }
}

int main(int argc, char** argv){

    if(argc < 2){
        std::cout << "No input file, aborting." << std::endl;
        return 0;
    }

    std::string configfile = argv[1];

    Config cfg;
    try{
        cfg = Config::CreateFromFile(configfile);
    }catch(ConfigFileException ex){
        std::cout << "Failed to load config file: " << ex.what() << std::endl;
        return 1;
    }

    Assimp::Importer importer;

    std::string configdir = Utils::GetDir(configfile);
    std::string modelfile = configdir + "/" + cfg.model_file;
    if(!Utils::GetFileExists(modelfile)){
        std::cout << "Unable to find model file `" << modelfile << "`. " << std::endl;
        return 1;
    }

    const aiScene* scene = importer.ReadFile(modelfile,
                                             aiProcessPreset_TargetRealtime_MaxQuality
                      );

    if(!scene){
        std::cout << "Assimp failed to load scene `" << modelfile << "`. " << std::endl;
        return 1;
    }

    std::cout << "Loaded scene with " << scene->mNumMeshes << " meshes, " <<
        scene->mNumMaterials << " materials and " << scene->mNumLights <<
        " lights." << std::endl;

    Scene s;
    s.LoadScene(scene);
    s.Commit();

    CameraConfig cconfig(cfg.view_point, cfg.look_at, cfg.up_vector, cfg.yview, cfg.yview*cfg.xres/cfg.yres);

    OutBuffer ob(cfg.xres, cfg.yres);

    ctpl::thread_pool tpool(7);

    // Split rendering into smaller (100x100) tasks.
    for(unsigned int yp = 0; yp < cfg.yres; yp += 100){
        for(unsigned int xp = 0; xp < cfg.xres; xp += 100){
            RenderTask task;
            task.xres = cfg.xres; task.yres = cfg.yres;
            task.xrange_start = xp; task.xrange_end = std::min(cfg.xres, xp+100);
            task.yrange_start = yp; task.yrange_end = std::min(cfg.yres, yp+100);
            task.scene = &s;
            task.cconfig = &cconfig;
            task.lights = &cfg.lights;
            task.multisample = cfg.multisample;
            task.recursion_level = cfg.recursion_level;
            task.output = &ob;
            tpool.push( [task](int){Render(task);} );
        }
    }

    tpool.stop(true); // Waits for all remaining threads to complete.

    //ob.WriteToBMP(cfg.output_file);
    ob.WriteToPNG(cfg.output_file);

    return 0;
}
