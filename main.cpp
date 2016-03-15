#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>
#include <queue>
#include <cmath>
#include <thread>

#include "external/ctpl_stl.h"

#include "scene.hpp"
#include "ray.hpp"
#include "texture.hpp"
#include "config.hpp"
#include "utils.hpp"
#include "texture.hpp"

static bool debug_trace = false;
static unsigned int debug_x, debug_y;

Color trace_ray(const Scene& scene, const Ray& r, const std::vector<Light>& lights, int depth, bool debug = false){
    if(debug) std::cerr << "Debugging a ray. " << std::endl;
    Intersection i = scene.FindIntersectKd(r, debug);

    if(i.triangle){
        if(debug) std::cerr << "Intersection found. " << std::endl;
        const Material& mat = i.triangle->GetMaterial();
        Color total(0.0, 0.0, 0.0);

        glm::vec3 ipos = r[i.t];
        glm::vec3 N = i.Interpolate(i.triangle->GetNormalA(),
                                    i.triangle->GetNormalB(),
                                    i.triangle->GetNormalC());
        glm::vec3 V = -r.direction; // incoming direction

        if(debug) std::cerr << "Was hit. color is " << mat.diffuse << std::endl;

        glm::vec2 texUV;
        if(mat.ambient_texture || mat.diffuse_texture || mat.specular_texture){
            texUV = i.Interpolate(i.triangle->GetTexCoordsA(),
                                  i.triangle->GetTexCoordsB(),
                                  i.triangle->GetTexCoordsC());
        }

        Color diffuse  =  mat.diffuse_texture ?  mat.diffuse_texture->GetPixelInterpolated(texUV) : mat.diffuse ;
        Color specular = mat.specular_texture ? mat.specular_texture->GetPixelInterpolated(texUV) : mat.specular;
        Color ambient  =  mat.ambient_texture ?  mat.ambient_texture->GetPixelInterpolated(texUV) : mat.ambient ;

        for(const Light& l : lights){
            glm::vec3 L = glm::normalize(l.pos - ipos);
            bool shadow;
            if(depth == 0) shadow = false;
            else{
                // if no intersection on path to light
                Ray ray_to_light(ipos, l.pos, 0.01 * glm::length(ipos - l.pos));
                Intersection i2 = scene.FindIntersectKd(ray_to_light);
                shadow = i2.triangle;
            }
            if(!shadow){ // no intersection found
                //TODO: use interpolated normals

                float distance = glm::length(ipos - l.pos); // The distance to light
                float d = distance/l.intensity;
                float intens_factor = 1.0f/(1.0f + d*d); // Light intensity falloff function

                if(debug) std::cerr << "No shadow, distance: " << distance << std::endl;

                float kD = glm::dot(N, L);
                kD = glm::max(0.0f, kD);
                total += intens_factor * l.color * diffuse * kD;

                if(debug) std::cerr << "N " << N << std::endl;
                if(debug) std::cerr << "L " << L << std::endl;
                if(debug) std::cerr << "kD " << kD << std::endl;
                if(debug) std::cerr << "Total: " << total << std::endl;

                if(mat.exponent > 1.0f){
                    glm::vec3 R = 2.0f * glm::dot(L, N) * N - L;
                    float a = glm::dot(R, V);
                    a = glm::max(0.0f, a);
                    float kS = glm::pow(a, mat.exponent);
                    // if(std::isnan(kS)) std::cout << glm::dot(R,V) << "/" << mat.exponent << std::endl;
                    total += intens_factor * l.color * specular * kS * 1.0f;
                }
            }else{
                if(debug) std::cerr << "Shadow found." << std::endl;
            }
        }

        // Special case for when there is no light
        if(lights.size() == 0){
            total += diffuse;
        }

        // Ambient lighting
        total += ambient;

        // Next ray
        if(depth >= 2 && mat.exponent <= 1.0f){
            glm::vec3 refl = 2.0f * glm::dot(V, N) * N - V;
            Ray refl_ray(ipos, ipos + refl, 0.01);
            refl_ray.far = 1000.0f;
            Color reflection = trace_ray(scene, refl_ray, lights, depth-1);
            total = mat.exponent * reflection + (1.0f - mat.exponent) * total;
        }
        if(debug) std::cout << "Total: " << total << std::endl;
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
    Texture* output;
};

void Render(RenderTask task){
    unsigned int m = task.multisample;
    for(unsigned int y = task.yrange_start; y < task.yrange_end; y++){
        for(unsigned int x = task.xrange_start; x < task.xrange_end; x++){
            bool d = false;
            if(debug_trace && x == debug_x && y == debug_y) d = true;
            Color pixel_total(0.0, 0.0, 0.0);
            float factor = 1.0/(m*m);
            for(unsigned int my = 0; my < m; my++){
                for(unsigned int mx = 0; mx < m; mx++){
                    glm::vec3 p = task.cconfig->GetViewScreenPoint(x*m + mx, y*m + my, task.xres*m, task.yres*m);
                    Ray r(task.cconfig->camerapos, p - task.cconfig->camerapos);
                    pixel_total += trace_ray(*task.scene, r, *task.lights, task.recursion_level, d) * factor;
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

    if(argc >= 4){
        debug_trace = true;
        debug_x = std::stoi(argv[2]);
        debug_y = std::stoi(argv[3]);
        std::cout << "Debug mode enabled, will trace pixel " << debug_x << " " << debug_y << std::endl;
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
    std::string modeldir  = Utils::GetDir(modelfile);
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
    s.texture_directory = modeldir + "/";
    s.LoadScene(scene);
    s.Commit();

    CameraConfig cconfig(cfg.view_point, cfg.look_at, cfg.up_vector, cfg.yview, cfg.yview*cfg.xres/cfg.yres);

    Texture ob(cfg.xres, cfg.yres);

    unsigned int concurrency = std::thread::hardware_concurrency();
    concurrency = std::max((unsigned int)1, concurrency - 1); // If available, leave one core free.
    ctpl::thread_pool tpool(concurrency);

    std::cout << "Using thread pool of size " << concurrency << std::endl;

    if(cfg.recursion_level == 0){
        cfg.lights.clear();
    }

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
