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

#include <getopt.h>

#include "LRU.hpp"

#include "scene.hpp"
#include "ray.hpp"
#include "texture.hpp"
#include "config.hpp"
#include "utils.hpp"
#include "texture.hpp"
#include "camera.hpp"
#include "path_tracer.hpp"
#include "out.hpp"

#include "global_config.hpp"

std::atomic<int> rounds_done(0);
unsigned int total_rounds;
std::atomic<int> pixels_done(0);
std::atomic<unsigned int> raycount(0);
std::atomic<bool> stop_monitor(false);
int total_pixels;

bool preview_mode = false;

std::string float_to_percent_string(float f){
    std::stringstream ss;
    ss << std::setw(5) << std::fixed << std::setprecision(1) << f << "%";
    return ss.str();
}

void Monitor(){

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

    // Helper function that prints out the progress bar.
    auto print_progress_f = [](){
        int d = pixels_done;
        float fraction = d/(float)total_pixels;
        float percent = int(fraction*1000.0f + 0.5f) / 10.0f;
        unsigned int fill = fraction * BARSIZE;
        unsigned int empty = BARSIZE - fill;
        out::cout(1) << "\33[2K\rRendered " << std::setw(log10(total_pixels) + 1) << d << "/" << total_pixels << " pixels, [";
        for(unsigned int i = 0; i <  fill; i++) out::cout(1) << "#";
        for(unsigned int i = 0; i < empty; i++) out::cout(1) << "-";
        out::cout(1) << "] " <<  float_to_percent_string(percent) << " done; round " << std::min((unsigned int)rounds_done + 1, total_rounds) << "/" << total_rounds;
        out::cout(1).flush();
    };

    while(!stop_monitor){
        print_progress_f();
        if(pixels_done >= total_pixels) break;
        usleep(1000*100); // 100ms
    }

    // Output the message one more time to display "100%"
    print_progress_f();
    std::cout << std::endl;

    // Measure total wallclock time
    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    float total_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0f;
    unsigned int total_rays = raycount;

    out::cout(2) << "Total rendering time: " << total_seconds << "s = " << (int)(0.5 + total_seconds/60.0) << " min" << std::endl;
    out::cout(3) << "Total rays: " << total_rays << std::endl;
    out::cout(2) << "Average pixels per second: " << Utils::FormatIntThousands(total_pixels / total_seconds) << "." << std::endl;
    out::cout(3) << "Average rays per second: " << Utils::FormatIntThousands(total_rays / total_seconds) << std::endl;

}

void usage(const char* prog) __attribute__((noreturn));
void usage(const char* prog){
    std::cout << "Usage: " << prog << " [OPTIONS]... [FILE] \n";
    std::cout << "\n";
    std::cout << "Runs the RGK Ray Tracer using configuration from FILE.\n";
    std::cout << " -p, --preview      Renders a preview (" << PREVIEW_DIMENTIONS_RATIO << "x smaller dimentions, " << PREVIEW_RAYS_RATIO << " times less rays per pixel, yielding " << PREVIEW_SPEED_RATIO << "\n";
    std::cout << "                       times faster render time).\n";
    std::cout << " -v                 Each occurence of this option increases verbosity by 1.\n";
    std::cout << " -q                 Each occurence of this option decreases verbosity by 1.\n";
    std::cout << "                      Default verbosity level is 2. At 0, the program operates quietly.\n";
    std::cout << "                      Increasing verbosity makes the program output more statistics and diagnostic details.\n";
#if ENABLE_DEBUG
    std::cout << " -d, --debug X Y    Prints verbose debug information about rendering the X Y pixel.\n";
#else
    std::cout << " -d                 (unavailable) Debugging support was disabled compile-time.\n";
#endif // ENABLE_DEBUG
    std::cout << " -h, --help         Prints out this message.\n";
    std::cout << "\n";
    exit(0);
}

int main(int argc, char** argv){

    static struct option long_opts[] =
        {
#if ENABLE_DEBUG
            {"debug", no_argument, 0, 'd'},
#endif // ENABLE_DEBUG
            {"preview", no_argument, 0, 'p'},
            {"help", no_argument, 0, 'h'},
            {0,0,0,0}
        };

    // Recognize command-line arguments
    int c;
    int opt_index = 0;
#if ENABLE_DEBUG
    #define OPTSTRING "hpd:vq"
#else
    #define OPTSTRING "hpvq"
#endif
    while((c = getopt_long(argc,argv,OPTSTRING,long_opts,&opt_index)) != -1){
        switch (c){
        case 'h':
            usage(argv[0]);
            break;
#if ENABLE_DEBUG
        case 'd':
            debug_trace = true;
            debug_x = std::stoi(optarg);
            if (optind < argc && *argv[optind] != '-'){
                debug_y = std::stoi(argv[optind]);
                optind++;
            } else {
                std::cout << "ERROR: Not enough arguments for --debug.\n";
                usage(argv[0]);
            }
            break;
#endif // ENABLE_DEBUG
        case 'p':
            preview_mode = true;
            break;
        case 'v':
            out::verbosity_level++;
            break;
        case 'q':
            if(out::verbosity_level > 0) out::verbosity_level--;
            break;
        default:
            std::cout << "ERROR: Unrecognized option " << (char)c << std::endl;
            /* FALLTHROUGH */
        case 0:
            usage(argv[0]);
        }
    }

    // Get the input file name from command line
    std::vector<std::string> infiles;
    while(optind < argc){
        infiles.push_back(argv[optind]);
        optind++;
    }
    if(infiles.size() < 1){
        std::cout << "ERROR: Missing FILE argument." << std::endl;
        usage(argv[0]);
    }else if(infiles.size() > 1){
        std::cout << "ERROR: Working with multiple config files is not yet supported." << std::endl;
        usage(argv[0]);
    }
    std::string configfile = infiles[0];

    // Load render config file
    Config cfg;
    try{
        cfg = Config::CreateFromFile(configfile);
    }catch(ConfigFileException ex){
        std::cout << "Failed to load config file: " << ex.what() << std::endl;
        return 1;
    }

    // Enable preview mode
    if(preview_mode){
        cfg.xres /= PREVIEW_DIMENTIONS_RATIO;
        cfg.yres /= PREVIEW_DIMENTIONS_RATIO;
        cfg.multisample /= PREVIEW_RAYS_RATIO;
    }

    // Prepare file paths
    std::string configdir = Utils::GetDir(configfile);
    std::string modelfile = configdir + "/" + cfg.model_file;
    std::string modeldir  = Utils::GetDir(modelfile);
    if(!Utils::GetFileExists(modelfile)){
        std::cout << "Unable to find model file `" << modelfile << "`. " << std::endl;
        return 1;
    }

    // Load the model with assimp
    Assimp::Importer importer;
    out::cout(2) << "Loading scene... " << std::endl;
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE, nullptr);
    const aiScene* scene = importer.ReadFile(modelfile,
                                             aiProcess_Triangulate |
                                             //aiProcess_TransformUVCoords |
                                             //aiProcess_GenNormals |
                                             aiProcess_GenSmoothNormals |
                                             aiProcess_JoinIdenticalVertices |

                                             // The option below is odd. Enabling it messes up half of my models.
                                             // Enabling it messes up the other half.
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

    out::cout(2) << "Loaded scene with " << scene->mNumMeshes << " meshes and " <<
        scene->mNumMaterials << " materials." << std::endl;

    // Prepare the world.
    Scene s;
    s.texture_directory = modeldir + "/";
    s.LoadScene(scene, cfg);
    s.Commit();

    s.AddPointLights(cfg.lights);

    Camera camera(cfg.view_point,
                  cfg.look_at,
                  cfg.up_vector,
                  cfg.yview,
                  cfg.yview*cfg.xres/cfg.yres,
                  cfg.focus_plane,
                  cfg.lens_size
                  );

    auto thinglass_materialset = s.MakeMaterialSet(cfg.thinglass);

    // Determine thread pool size.
    unsigned int concurrency = std::thread::hardware_concurrency();
    concurrency = std::max((unsigned int)1, concurrency - 1); // If available, leave one core free.
    out::cout(2) << "Using thread pool of size " << concurrency << std::endl;

    std::string output_file = (preview_mode) ? Utils::InsertFileSuffix(cfg.output_file, "preview") : cfg.output_file;
    out::cout(2) << "Writing to file " << output_file << std::endl;

    // Split rendering into smaller (tile_size x tile_size) tasks.
    std::vector<RenderTask> tasks;
    for(unsigned int yp = 0; yp < cfg.yres; yp += TILE_SIZE){
        for(unsigned int xp = 0; xp < cfg.xres; xp += TILE_SIZE){
            RenderTask task(cfg.xres, cfg.yres, xp, std::min(cfg.xres, xp+TILE_SIZE),
                                                yp, std::min(cfg.yres, yp+TILE_SIZE));
            tasks.push_back(task);
        }
    }

    out::cout(3) << "Rendering in " << tasks.size() << " tiles." << std::endl;

    // Sorting tasks by their distance to the middle.
    glm::vec2 middle(cfg.xres/2.0f, cfg.yres/2.0f);
#if ENABLE_DEBUG
    // However, if debug is enabled, sort tiles so that the debugged point gets rendered earliest.
    if(debug_trace) middle = glm::vec2(debug_x, debug_y);
#endif
    std::sort(tasks.begin(), tasks.end(), [&middle](const RenderTask& a, const RenderTask& b){
            return glm::length(middle - a.midpoint) < glm::length(middle - b.midpoint);
        });

    // Preapare output buffer
    EXRTexture ob(cfg.xres, cfg.yres);

    // Start monitor thread.
    total_pixels = cfg.xres * cfg.yres * cfg.rounds;
    total_rounds = cfg.rounds;
    std::thread monitor_thread(Monitor);

    // Repeat for each rendering round.
    unsigned int seedcount = 0, seedstart = time(nullptr);
    for(unsigned int roundno = 0; roundno < cfg.rounds; roundno++){
        ctpl::thread_pool tpool(concurrency);

        // Push all render tasks to thread pool
        for(const RenderTask& task : tasks){
            unsigned int c = seedcount++;
            tpool.push( [&ob, seedstart, camera, &s, cfg, task, c, &thinglass_materialset](int){

                    Random rnd(seedstart + c);
                    PathTracer rt(s, camera,
                                  task.xres, task.yres,
                                  cfg.multisample,
                                  cfg.recursion_level,
                                  cfg.sky_color,
                                  cfg.sky_brightness,
                                  cfg.clamp,
                                  cfg.russian,
                                  cfg.bumpmap_scale,
                                  thinglass_materialset,
                                  rnd);

                    rt.Render(task, &ob, pixels_done, raycount);

                });
        }

        // Wait for all remaining worker threads to complete.
        tpool.stop(true);

        rounds_done++;

        // Write out current progress to the output file.
        ob.Normalize().Write(output_file);
    }

    // Shutdown monitor thread.
    stop_monitor = true;
    if(monitor_thread.joinable()) monitor_thread.join();

    return 0;
}
