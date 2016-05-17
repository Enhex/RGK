#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>

#include <iostream>
#include <iomanip>
#include <queue>
#include <cmath>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sstream>

#include "../external/ctpl_stl.h"

#include "LRU.hpp"

#include "scene.hpp"
#include "ray.hpp"
#include "texture.hpp"
#include "config.hpp"
#include "utils.hpp"
#include "texture.hpp"
#include "camera.hpp"
#include "path_tracer.hpp"

bool debug_trace = false;
unsigned int debug_x, debug_y;

#define TILE_SIZE 50

std::atomic<int> tasks_done(0);
std::atomic<int> pixels_done(0);
std::atomic<unsigned int> raycount(0);
std::atomic<bool> stop_monitor(false);
int total_pixels;

std::string float_to_percent_string(float f){
    std::stringstream ss;
    ss << std::setw(5) << std::fixed << std::setprecision(1) << f << "%";
    return ss.str();
}

void Monitor(const EXRTexture* output_buffer, std::string preview_path){
    std::cout << "Monitor thread started" << std::endl;
    int counter = 0;

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    auto print_progress_f = [](){
        int d = pixels_done;
        float fraction = d/(float)total_pixels;
        float percent = int(fraction*1000.0f + 0.5f) / 10.0f;
        const unsigned int barsize = 60;
        unsigned int fill = fraction * barsize;
        unsigned int empty = barsize - fill;
        std::cout << "\33[2K\rRendered " << std::setw(log10(total_pixels) + 1) << d << "/" << total_pixels << " pixels, [";
        for(unsigned int i = 0; i <  fill; i++) std::cout << "#";
        for(unsigned int i = 0; i < empty; i++) std::cout << "-";
        std::cout << "] " <<  float_to_percent_string(percent) << " done.";
        std::flush(std::cout);
    };

    while(!stop_monitor){
        print_progress_f();
        if(pixels_done >= total_pixels) break;

        if(counter % 10 == 0){
            // Each second
            auto ob2 = output_buffer->Normalize();
            ob2.Write(preview_path);
        }

        usleep(1000*100); // 100ms
        counter++;
    }

    // Display the message one more time to output "100%"
    print_progress_f();
    std::cout << std::endl;
    output_buffer->Write(preview_path);

    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    float total_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0f;
    unsigned int total_rays = raycount;

    std::cout << "Total rendering time: " << total_seconds << "s" << std::endl;
    std::cout << "Total pixels: " << total_pixels << ", total rays: " << total_rays << std::endl;
    std::cout << "Average pixels per second: " << Utils::FormatIntThousands(total_pixels / total_seconds) << "." << std::endl;
    std::cout << "Average rays per second: " << Utils::FormatIntThousands(total_rays / total_seconds) << std::endl;

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

    srand(time(nullptr));

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

    std::cout << "Loading scene... " << std::endl;
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE, NULL);
    const aiScene* scene = importer.ReadFile(modelfile,
                                             aiProcess_Triangulate |
                                             //aiProcess_TransformUVCoords |
                                             //aiProcess_GenNormals |
                                             aiProcess_GenSmoothNormals |
                                             aiProcess_JoinIdenticalVertices |
                                             aiProcess_RemoveRedundantMaterials |
                                             aiProcess_GenUVCoords |
                                             //aiProcess_SortByPType |
                                             aiProcess_FindDegenerates |
                                             // DO NOT ENABLE THIS CLEARLY BUGGED SEE SIBENIK MODEL aiProcess_FindInvalidData |
                                             //aiProcess_ValidateDataStructure |
                     0 );

    if(!scene){
        std::cout << "Assimp failed to load scene `" << modelfile << "`: " << importer.GetErrorString() << std::endl;
        return 1;
    }

    // Calculating tangents is requested AFTER the scene is
    // loaded. Otherwise this step runs before normals are calculated
    // for vertices that are missing them.
    scene = importer.ApplyPostProcessing(aiProcess_CalcTangentSpace);
    //aiApplyPostProcessing(scene, aiProcess_CalcTangentSpace);

    std::cout << "Loaded scene with " << scene->mNumMeshes << " meshes, " <<
        scene->mNumMaterials << " materials and " << scene->mNumLights <<
        " lights." << std::endl;

    Scene s;
    s.texture_directory = modeldir + "/";
    s.LoadScene(scene);
    s.Commit();

    Camera camera(cfg.view_point,
                  cfg.look_at,
                  cfg.up_vector,
                  cfg.yview,
                  cfg.yview*cfg.xres/cfg.yres,
                  cfg.focus_plane,
                  cfg.lens_size
                  );

    EXRTexture ob(cfg.xres, cfg.yres);
    //ob.FillStripes(15, Color(0.6,0.6,0.6), Color(0.5,0.5,0.5));

    unsigned int concurrency = std::thread::hardware_concurrency();
    concurrency = std::max((unsigned int)1, concurrency - 1); // If available, leave one core free.
    ctpl::thread_pool tpool(concurrency);

    std::cout << "Using thread pool of size " << concurrency << std::endl;

    if(cfg.recursion_level == 0){
        cfg.lights.clear();
    }

    std::vector<RenderTask> tasks;

    total_pixels = cfg.xres * cfg.yres;

    auto p = Utils::GetFileExtension(cfg.output_file);
    std::thread monitor_thread(Monitor, &ob, p.first + ".preview." + p.second);

    const int tile_size = TILE_SIZE;
    // Split rendering into smaller (tile_size x tile_size) tasks.
    for(unsigned int yp = 0; yp < cfg.yres; yp += tile_size){
        for(unsigned int xp = 0; xp < cfg.xres; xp += tile_size){
            RenderTask task(cfg.xres, cfg.yres, xp, std::min(cfg.xres, xp+tile_size),
                                                yp, std::min(cfg.yres, yp+tile_size));
            tasks.push_back(task);
        }
    }

    std::cout << "Rendering in " << tasks.size() << " tiles." << std::endl;

    // Sorting tasks by their distance to the middle
    glm::vec2 middle(cfg.xres/2.0f, cfg.yres/2.0f);
    std::sort(tasks.begin(), tasks.end(), [&middle](const RenderTask& a, const RenderTask& b){
            return glm::length(middle - a.midpoint) < glm::length(middle - b.midpoint);
        });

    for(const RenderTask& task : tasks){
        tpool.push( [&, task](int){
                PathTracer rt(s, camera, cfg.lights,
                              task.xres, task.yres,
                              cfg.multisample,
                              cfg.recursion_level,
                              cfg.sky_color,
                              cfg.sky_brightness,
                              cfg.clamp,
                              cfg.russian,
                              cfg.bumpmap_scale);

                rt.Render(task, &ob, pixels_done, raycount);

                tasks_done++;
            });
    }

    tpool.stop(true); // Waits for all remaining worker threads to complete.

    stop_monitor = true;
    if(monitor_thread.joinable()) monitor_thread.join();

    auto ob2 = ob.Normalize();
    ob2.Write(cfg.output_file);

    return 0;
}
