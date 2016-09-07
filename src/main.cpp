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
 -r, --rotate      Renders a set of images, rotating the camera around the
                     lookat point. Temporary substitute for a flying camera.
 -t MINUTES,       Forces a predetermined render time, ignoring time and rounds
 --timed MINUTES     settings from the scene configuration file.
 --no-overwrite    Aborts rendering if the output file already exists. Useful
                     for rendering on multiple machines that share filesystem.
 -D, --dir DIR     The directory where output file will be stored.

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
            {"dir", required_argument, 0, 'D'},
            {"rotate", no_argument, 0, 'r'},
            {"timed", required_argument, 0, 't'},
            {"preview", no_argument, 0, 'p'},
            {"help", no_argument, 0, 'h'},
            {"no-overwrite", no_argument, &no_overwrite, true},
            {0,0,0,0}
        };

    // Recognize command-line arguments
    int c;
    bool rotate = false;
    bool force_timed = false; int force_timed_minutes = 0;
    bool preview_mode = false;
    bool compare_mode = false;
    std::string directory = "";
    int opt_index = 0;
#if ENABLE_DEBUG
    #define OPTSTRING "hpcvqrt:d:"
#else
    #define OPTSTRING "hpcvqrt:"
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
            break;
        case 't':
            force_timed = true;
            force_timed_minutes = std::stoi(optarg);
            if(force_timed_minutes <= 0){
                std::cout << "ERROR: Invalid argument for -t (--time).\n";
                usage(argv[0]);
            }
            break;
        case 'D':
            directory = optarg;
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

    // If requested, force timed mode
    if(force_timed){
        //out::cout(2) << "Command line forced render time to " << Utils::FormatTime(force_timed_minutes*60) << " minutes." << std::endl;
        cfg->render_limit_mode = RenderLimitMode::Timed;
        cfg->render_minutes = force_timed_minutes;
    }

    // Prepare output file name
    std::string output_file = directory + "/" + cfg->output_file;
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
        scene.MakeThinglassSet(cfg->thinglass);
    }catch(ConfigFileException ex){
        std::cout << "Failed to load data from config file: " << ex.what() << std::endl;
        return 1;
    }
    scene.Commit();

    // Prepare camera.
    Camera camera = cfg->GetCamera(0.0f);

    // The config file is not parsed anymore from this point on. This
    // is a good moment to warn user about e.g. unused keys
    cfg->PerformPostCheck();

    // TODO: Configurable FPS and length
    float fps = 50.0;
    float time_length = 0.0f, time_increment = 1.0f/fps;
    if(rotate) time_length = 2.0f;

    bool animated = (time_length > 0.0f);

    std::string base_output_file = output_file;

    unsigned int frame_no = -1;
    for(float t = 0.0f; t <= time_length; t += time_increment){
        frame_no++;

        if(animated) output_file = Utils::InsertFileSuffix(base_output_file, Utils::FormatInt5(frame_no));
        else output_file = base_output_file;

        if(no_overwrite && Utils::GetFileExists(output_file)){
            out::cout(1) << "File `" << output_file << "` exists, not overwriting." << std::endl;
            continue;
        }
        if(animated){
            float fraction = t / time_length;
            out::cout(1) << "Rendering frame #" << frame_no << " of ~" << int(time_length*fps) << " (" << Utils::FormatPercent(fraction*100.0f) << ")" << std::endl;
        }

        Camera c = camera;
        if(rotate){
            c = cfg->GetCamera(t / time_length);
        }

        RenderDriver::RenderFrame(scene, cfg, c, output_file);
    }

    return 0;
}
