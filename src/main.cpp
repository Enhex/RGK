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

#include "global_config.hpp"

#include "scene.hpp"
#include "ray.hpp"
#include "texture.hpp"
#include "config.hpp"
#include "utils.hpp"
#include "texture.hpp"
#include "camera.hpp"
#include "path_tracer.hpp"
#include "out.hpp"

#include "LTC/ltc.hpp"

std::chrono::high_resolution_clock::time_point render_start;
std::atomic<bool> stop_monitor(false);

// Performance counters
std::atomic<int> rounds_done(0);
std::atomic<int> pixels_done(0);
std::atomic<unsigned int> rays_done(0);

bool preview_mode = false;
bool compare_mode = false;

std::string float_to_percent_string(float f){
    std::stringstream ss;
    ss << /*std::setw(5) <<*/ std::fixed << std::setprecision(1) << f << "%";
    return ss.str();
}

void Monitor(RenderLimitMode render_limit_mode,
             unsigned int limit_rounds,
             unsigned int limit_minutes,
             unsigned int pixels_per_round){

    // Helper function that prints out the progress bar.
    auto print_progress_f = [=](bool final_print = false){
        static Utils::LowPass eta_lp(40);

        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        float elapsed_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - render_start).count() / 1000.0f;
        float elapsed_minutes = elapsed_seconds / 60.0f;

        // Prepare variables and text that varies with render limit mode
        float fraction = 0.0f, eta_seconds = 0.0f;
        std::string pixels_text, rounds_text;
        std::stringstream ss;
        bool mask_eta = false;
        switch(render_limit_mode){
        case RenderLimitMode::Rounds:
            const static unsigned int total_pixels = pixels_per_round * limit_rounds;
            fraction = pixels_done/(float)total_pixels;
            eta_seconds = (1.0f - fraction)*elapsed_seconds/fraction;
            // Smooth ETA with a simple low-pass filter
            eta_seconds = eta_lp.Add(eta_seconds);
            ss << "Rendered " << std::setw(log10(total_pixels) + 1) << pixels_done << "/" << total_pixels << " pixels";
            pixels_text = ss.str();
            ss = std::stringstream();
            ss << "round " << std::min((unsigned int)rounds_done + 1, limit_rounds) << "/" << limit_rounds;
            rounds_text = ss.str();
            mask_eta = (fraction < 0.03f && elapsed_seconds < 20.0f);
            break;
        case RenderLimitMode::Timed:
            fraction = std::min(1.0f, elapsed_minutes/limit_minutes);
            eta_seconds = std::max(0.0f, limit_minutes*60 - elapsed_seconds);
            pixels_text = "Rendered " + std::to_string(pixels_done) + " pixels";
            ss << "round " << rounds_done + (final_print?0:1);
            rounds_text = ss.str();
            mask_eta = false;
            break;
        }

        float percent = int(fraction*1000.0f + 0.5f) / 10.0f;
        // If this is the last time we're displayin the bar, make sure to nicely round numbers
        if(final_print){
            eta_seconds = 0.0f;
            if(render_limit_mode == RenderLimitMode::Rounds){
                fraction = 1.0f;
                percent = 100.0f;
            }
        }
        std::string eta_str = "ETA: " + std::string(((mask_eta)?"???":Utils::FormatTime(eta_seconds)));
        std::string percent_str = float_to_percent_string(percent);
        std::string elapsed_str = "time elapsed: " + Utils::FormatTime(elapsed_seconds);
        unsigned int bar_fill = fraction * BARSIZE;
        unsigned int bar_empty = BARSIZE - bar_fill;
        // Output text
        out::cout(1) << "\033[1A"; // Cursor up 1 line
        // Line 1
        out::cout(1) << "\33[2K\r" << "[";
        for(unsigned int i = 0; i < bar_fill ; i++) out::cout(1) << "#";
        for(unsigned int i = 0; i < bar_empty; i++) out::cout(1) << "-";
        out::cout(1) << "] " << percent_str << std::endl;
        // Line 2
        out::cout(1) << "\33[2K\r" << pixels_text << ", " << rounds_text << ", " << elapsed_str << ", " << eta_str;
        out::cout(1).flush();
    };

    while(!stop_monitor){
        print_progress_f();
        //if(pixels_done >= total_pixels) break;
        usleep(1000*100); // 50ms
    }

    // Output the message one more time to display "100%"
    print_progress_f(true);
    std::cout << std::endl;

    // Measure total wallclock time
    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    float total_seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - render_start).count() / 1000.0f;

    out::cout(2) << "Total rendering time: " << Utils::FormatTime(total_seconds) << std::endl;
    out::cout(3) << "Total rays: " << rays_done << std::endl;
    out::cout(2) << "Average pixels per second: " << Utils::FormatIntThousands(pixels_done / total_seconds) << "." << std::endl;
    out::cout(3) << "Average rays per second: " << Utils::FormatIntThousands(rays_done / total_seconds) << std::endl;

}

// TODO: Thinglass material set should not be an argument here.
// TODO: Seed generator should be a standalone object
// TODO: Create a different 'lite config' struct, which will only
// contain render parameters, ideal for passing here
void RenderRound(const Scene& scene,
                 std::shared_ptr<Config> cfg,
                 const Camera& camera,
                 const std::vector<RenderTask>& tasks,
                 std::set<const Material*> thinglass_materialset,
                 unsigned int& seedcount,
                 const int seedstart,
                 unsigned int concurrency,
                 EXRTexture& total_ob
                 ){

    ctpl::thread_pool tpool(concurrency);

    // Prepare per-task output buffers
    std::vector<EXRTexture> output_buffers(tasks.size(), EXRTexture(cfg->xres, cfg->yres));

    // Push all render tasks to thread pool
    for(unsigned int i = 0; i < tasks.size(); i++){
        const RenderTask& task = tasks[i];
        EXRTexture& output_buffer = output_buffers[i];
        unsigned int c = seedcount++;
        tpool.push( [&output_buffer, seedstart, camera, &scene, &cfg, task, c, &thinglass_materialset](int){

                // THIS is the thread task
                Random rnd(seedstart + c);
                PathTracer rt(scene, camera,
                              task.xres, task.yres,
                              cfg->multisample,
                              cfg->recursion_level,
                              cfg->clamp,
                              cfg->russian,
                              cfg->bumpmap_scale,
                              cfg->force_fresnell,
                              cfg->reverse,
                              thinglass_materialset,
                              rnd);
                out::cout(6) << "Starting a new task with params: " << std::endl;
                out::cout(6) << "camerapos = " << camera.origin << ", multisample = " << cfg->multisample << ", reclvl = " << cfg->recursion_level << ", russian = " << cfg->russian << ", reverse = " << cfg->reverse << std::endl;

                rt.Render(task, &output_buffer, pixels_done, rays_done);

            });
    }

    // Wait for all remaining worker threads to complete.
    tpool.stop(true);

    rounds_done++;

    // Merge all outputs into a single one
    for(const EXRTexture& o : output_buffers)
        total_ob.Accumulate(o);
}

std::string usage_text = R"--(
Runs the RGK Ray Tracer using scene configuration from FILE.
 -p, --preview     Renders a fast preview, using smaller dimentions and less
                     samples per pixel than the target image.
 -v                Each occurrence of this option increases verbosity by 1.
 -q                Each occurrence of this option decreases verbosity by 1.
                     Default verbosity level is 2. At 0, the program operates
                     quietly. Increasig verbosity causes the program to output
                     more statistics and diagnostic details.
 -r N M            Rotates the camera by N/M of full angle around the lookat
                     point. Temporary substitute for a flying camera.
 -t MINUTES,       Forces a predetermined render time, ignoring time and rounds
 --timed MINUTES     settings from the scene configuration file.

 -h, --help        Prints out this message.
)--";
std::string debug_option_text = R"--(
 -d, --debug X Y   Prints debug information while rendering the X Y pixel.
)--";
std::string debug_disabled_text = R"--(
 -d                (unavailable) Debugging support was disabled compile-time.
)--";

void usage(const char* prog) __attribute__((noreturn));
void usage(const char* prog){
    std::cout << "Usage: " << prog << " [OPTIONS]... [FILE] \n";
    std::cout << usage_text;
#if ENABLE_DEBUG
    std::cout << debug_option_text;
#else
    std::cout <<debug_disabled_text;
#endif // ENABLE_DEBUG
    exit(0);
}

int main(int argc, char** argv){

    static struct option long_opts[] =
        {
#if ENABLE_DEBUG
            {"debug", required_argument, 0, 'd'},
#endif // ENABLE_DEBUG
            {"rotate", required_argument, 0, 'r'},
            {"timed", required_argument, 0, 't'},
            {"preview", no_argument, 0, 'p'},
            {"help", no_argument, 0, 'h'},
            {0,0,0,0}
        };

    // Recognize command-line arguments
    int c;
    bool rotate = false; int rotate_N = 0, rotate_M = 0;
    bool force_timed = false; int force_timed_minutes = 0;
    int opt_index = 0;
#if ENABLE_DEBUG
    #define OPTSTRING "hpcvqr:t:d:"
#else
    #define OPTSTRING "hpcvqr:t:"
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
        case 'r':
            rotate = true;
            rotate_N = std::stoi(optarg);
            if (optind < argc && *argv[optind] != '-'){
                rotate_M = std::stoi(argv[optind]);
                optind++;
            } else {
                std::cout << "ERROR: Not enough arguments for --rotate.\n";
                usage(argv[0]);
            }
            break;
        case 't':
            force_timed = true;
            force_timed_minutes = std::stoi(optarg);
            if(force_timed_minutes <= 0){
                std::cout << "ERROR: Invalid argument for -t (--time).\n";
                usage(argv[0]);
            }
            break;
        case 'p':
            preview_mode = true;
            break;
        case 'c':
            compare_mode = true;
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
    std::shared_ptr<Config> cfg;
    std::string cfg_ext;
    std::tie(std::ignore, cfg_ext) = Utils::GetFileExtension(configfile);
    try{
        if(cfg_ext == "rtc"){
            cfg = ConfigRTC::CreateFromFile(configfile);
        }else if(cfg_ext == "json"){
            cfg = ConfigJSON::CreateFromFile(configfile);
        }else{
            std::cout << "Config file format \"" << cfg_ext << "\" not recognized" << std::endl;
            return 1;
        }
    }catch(ConfigFileException ex){
        std::cout << "Failed to load config file: " << ex.what() << std::endl;
        return 1;
    }

    // Prepare camera rotation, needed for generating file name.
    float rotate_frac = 0.0f;
    if(rotate){
        if(rotate_M == 0){
            std::cout << "Invalid argument to --rotate, cannot divide by zero" << std::endl;
            return 1;
        }
        rotate_frac = (float)rotate_N/rotate_M;
    }
    // If requested, force timed mode
    if(force_timed){
        out::cout(2) << "Command line forced render time to " << Utils::FormatTime(force_timed_minutes*60) << " minutes." << std::endl;
        cfg->render_limit_mode = RenderLimitMode::Timed;
        cfg->render_minutes = force_timed_minutes;
    }

    // Prepare output file name
    std::string output_file = cfg->output_file;
    if(rotate) output_file = Utils::InsertFileSuffix(output_file, Utils::FormatFraction5(rotate_frac));
    if(preview_mode) output_file = Utils::InsertFileSuffix(output_file, "preview");
    if(compare_mode) output_file = Utils::InsertFileSuffix(output_file, "cmp");
    if(rotate && Utils::GetFileExists(output_file)){
        std::cout << "Not overwriting existing file in rotate mode: " << output_file << std::endl;
        return 1;
    }

    // Enable preview mode
    if(preview_mode){
        cfg->xres /= PREVIEW_DIMENTIONS_RATIO;
        cfg->yres /= PREVIEW_DIMENTIONS_RATIO;
        cfg->multisample /= PREVIEW_RAYS_RATIO;
    }

    // Preapare output buffer
    EXRTexture total_ob(cfg->xres, cfg->yres);
    total_ob.Write(output_file);

    // Prepare the scene
    Scene scene;
    try{
        cfg->InstallMaterials(scene);
        cfg->InstallScene(scene);
        cfg->InstallLights(scene);
        cfg->InstallSky(scene);
    }catch(ConfigFileException ex){
        std::cout << "Failed to load data from config file: " << ex.what() << std::endl;
        return 1;
    }
    scene.Commit();

    // Prepare camera.
    Camera camera = cfg->GetCamera(rotate_frac);

    std::set<const Material*> thinglass_materialset = scene.MakeMaterialSet(cfg->thinglass);

    // The config file is not parsed anymore from this point on. This
    // is a good moment to warn user about e.g. unused keys
    cfg->PerformPostCheck();

    out::cout(2) << "Writing to file " << output_file << std::endl;

    // Determine thread pool size.
    unsigned int concurrency = std::thread::hardware_concurrency();
    concurrency = std::max((unsigned int)1, concurrency - 1); // If available, leave one core free.
    out::cout(2) << "Using thread pool of size " << concurrency << std::endl;

    // Split rendering into smaller (tile_size x tile_size) tasks.
    std::vector<RenderTask> tasks;
    for(unsigned int yp = 0; yp < cfg->yres; yp += TILE_SIZE){
        for(unsigned int xp = 0; xp < cfg->xres; xp += TILE_SIZE){
            RenderTask task(cfg->xres, cfg->yres, xp, std::min(cfg->xres, xp+TILE_SIZE),
                                                  yp, std::min(cfg->yres, yp+TILE_SIZE));
            tasks.push_back(task);
        }
    }

    out::cout(3) << "Rendering in " << tasks.size() << " tiles." << std::endl;

    // Sorting tasks by their distance to the middle.
    glm::vec2 middle(cfg->xres/2.0f, cfg->yres/2.0f);
#if ENABLE_DEBUG
    // However, if debug is enabled, sort tiles so that the debugged point gets rendered earliest.
    if(debug_trace) middle = glm::vec2(debug_x, debug_y);
#endif
    std::sort(tasks.begin(), tasks.end(), [&middle](const RenderTask& a, const RenderTask& b){
            return glm::length(middle - a.midpoint) < glm::length(middle - b.midpoint);
        });

    // Start monitor thread.
    std::thread monitor_thread(Monitor, cfg->render_limit_mode, cfg->render_rounds, cfg->render_minutes,
                               cfg->xres * cfg->yres);

    unsigned int seedcount = 0, seedstart = 42;

    // Measuring render time, both for timed mode, and monitor output
    render_start = std::chrono::high_resolution_clock::now();

    switch(cfg->render_limit_mode){
    case RenderLimitMode::Rounds:
        for(unsigned int roundno = 0; roundno < cfg->render_rounds; roundno++){
            // Render a single round
            RenderRound(scene, cfg, camera, tasks, thinglass_materialset, seedcount, seedstart, concurrency, total_ob);
            // Write out current progress to the output file.
            total_ob.Normalize().Write(output_file);
        }
        break;
    case RenderLimitMode::Timed:
        while(true){
            // Break loop if too much time elapsed
            std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
            float minutes_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - render_start).count() / 60.0;
            if(minutes_elapsed >= cfg->render_minutes) break;
            // Render a single round
            RenderRound(scene, cfg, camera, tasks, thinglass_materialset, seedcount, seedstart, concurrency, total_ob);
            // Write out current progress to the output file.
            total_ob.Normalize().Write(output_file);
        }
        break;
    }

    // Shutdown monitor thread.
    stop_monitor = true;
    if(monitor_thread.joinable()) monitor_thread.join();

    return 0;
}
