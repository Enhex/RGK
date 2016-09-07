#include <iostream>
#include <sstream>

#include <getopt.h>

#include "global_config.hpp"

#include "scene.hpp"
#include "texture.hpp"
#include "config.hpp"
#include "utils.hpp"
#include "camera.hpp"
#include "out.hpp"
#include "render_driver.hpp"

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
 --no-overwrite    Aborts rendering if the output file already exists. Useful
                     for rendering on multiple machines that share filesystem.

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

    int no_overwrite = false;
    static struct option long_opts[] =
        {
#if ENABLE_DEBUG
            {"debug", required_argument, 0, 'd'},
#endif // ENABLE_DEBUG
            {"rotate", required_argument, 0, 'r'},
            {"timed", required_argument, 0, 't'},
            {"preview", no_argument, 0, 'p'},
            {"help", no_argument, 0, 'h'},
            {"no-overwrite", no_argument, &no_overwrite, true},
            {0,0,0,0}
        };

    // Recognize command-line arguments
    int c;
    bool rotate = false; int rotate_N = 0, rotate_M = 0;
    bool force_timed = false; int force_timed_minutes = 0;
    bool preview_mode = false;
    bool compare_mode = false;
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
            no_overwrite = true;
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
            usage(argv[0]);
            break;
        case 0:
            // NOP
            break;
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
        //out::cout(2) << "Command line forced render time to " << Utils::FormatTime(force_timed_minutes*60) << " minutes." << std::endl;
        cfg->render_limit_mode = RenderLimitMode::Timed;
        cfg->render_minutes = force_timed_minutes;
    }

    // Prepare output file name
    std::string output_file = cfg->output_file;
    if(preview_mode) output_file = Utils::InsertFileSuffix(output_file, "preview");
    if(compare_mode) output_file = Utils::InsertFileSuffix(output_file, "cmp");

    // Enable preview mode
    if(preview_mode){
        cfg->xres /= PREVIEW_DIMENTIONS_RATIO;
        cfg->yres /= PREVIEW_DIMENTIONS_RATIO;
        cfg->multisample /= PREVIEW_RAYS_RATIO;
    }

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

    // TODO: This should be a property of scene
    std::set<const Material*> thinglass_materialset = scene.MakeMaterialSet(cfg->thinglass);

    // The config file is not parsed anymore from this point on. This
    // is a good moment to warn user about e.g. unused keys
    cfg->PerformPostCheck();

    if(rotate) output_file = Utils::InsertFileSuffix(output_file, Utils::FormatFraction5(rotate_frac));
    if(no_overwrite && Utils::GetFileExists(output_file)){
        std::cout << "File `" << output_file << "` exists, not overwriting." << std::endl;
        return 1;
    }

    RenderDriver::RenderFrame(scene, cfg, camera, output_file, thinglass_materialset);

    RenderDriver::RenderFrame(scene, cfg, camera, output_file, thinglass_materialset);

    return 0;
}
