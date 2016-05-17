#include "config.hpp"

#include <fstream>

#include "utils.hpp"

#define NEXT_LINE()                                                                  \
    do{ std::getline(file, line);                                                    \
        line = Utils::Trim(line);                                                    \
        if(!file.good()) throw ConfigFileException("Config file ends prematurely."); \
    }while(0)

Config Config::CreateFromFile(std::string path){
    Config cfg;

    std::ifstream file(path, std::ios::in);
    if(!file) throw ConfigFileException("Failed to open config file ` " + path + " `.");

    std::string line;
    std::vector<std::string> vs;

    std::getline(file, line);  // comment
    line = Utils::Trim(line);
    cfg.comment = line;
    NEXT_LINE();  // model file
    cfg.model_file = line;
    NEXT_LINE();  // output file
    cfg.output_file = line;
    NEXT_LINE();  // recursion level
    unsigned int k = std::stoi(line);
    // if(k==0) throw ConfigFileException("Invalid k value.");
    cfg.recursion_level = k;
    NEXT_LINE(); // xres yres
    vs = Utils::SplitString(line," ");
    if(vs.size()!=2) throw ConfigFileException("Invalid resolution format.");
    unsigned int xres = std::stoi(vs[0]), yres = std::stoi(vs[1]);
    if(xres==0 || yres==0) throw ConfigFileException("Invalid output image resolution.");
    cfg.xres = xres;
    cfg.yres = yres;

    NEXT_LINE(); // VP
    vs = Utils::SplitString(line," ");
    if(vs.size()!=3) throw ConfigFileException("Invalid VP format.");
    float VPx = std::stof(vs[0]), VPy = std::stof(vs[1]), VPz = std::stof(vs[2]);
    cfg.view_point = glm::vec3(VPx, VPy, VPz);

    NEXT_LINE(); // LA
    vs = Utils::SplitString(line," ");
    if(vs.size()!=3) throw ConfigFileException("Invalid LA format.");
    float LAx = std::stof(vs[0]), LAy = std::stof(vs[1]), LAz = std::stof(vs[2]);
    // TODO: What it LA - VP = 0 ?
    cfg.look_at = glm::vec3(LAx, LAy, LAz);

    NEXT_LINE(); // UP
    vs = Utils::SplitString(line," ");
    if(vs.size()!=3) throw ConfigFileException("Invalid UP format.");
    float UPx = std::stof(vs[0]), UPy = std::stof(vs[1]), UPz = std::stof(vs[2]);
    cfg.up_vector = glm::vec3(UPx, UPy, UPz);

    NEXT_LINE(); // yview
    float yview = std::stof(line);
    if(yview <= 0.0f || yview >= 100.0f) throw ConfigFileException("Invalid yview value.");
    cfg.yview = yview;

    // Now for the extra lines
    line = "";
    do{
        std::getline(file, line);
        line = Utils::Trim(line);
        vs = Utils::SplitString(line," ");
        if(vs.size() == 0) continue;
        if(vs[0][0] == '#') continue;
        if(vs[0] == "") continue;
        if(vs[0] == "L"){
            // Light info
            if(vs.size() < 8 || vs.size() > 9) throw ConfigFileException("Invalid light line.");
            float l1 = std::stof(vs[1]), l2 = std::stof(vs[2]), l3 = std::stof(vs[3]);
            float c1 = std::stof(vs[4])/255, c2 = std::stof(vs[5])/255, c3 = std::stof(vs[6])/255;
            float i = std::stof(vs[7]);
            float s = 0.0f;
            if(vs.size() == 9) s = std::stof(vs[8]);
            cfg.lights.push_back(Light{glm::vec3(l1,l2,l3), Color(c1,c2,c3), i, s});
        }else if(vs[0] == "multisample" || vs[0] == "ms"){
            if(vs.size() != 2) throw ConfigFileException("Invalid multisample line.");
            int ms = std::stoi(vs[1]);
            if(ms == 0) throw ConfigFileException("Invalid multisample value.");
            cfg.multisample = ms;
        }else if(vs[0] == "sky" || vs[0] == "skycolor"){
            if(vs.size() < 4 || vs.size() > 5) throw ConfigFileException("Invalid sky color line.");
            cfg.sky_color = Color(std::stoi(vs[1])/255.0f, std::stoi(vs[2])/255.0f, std::stoi(vs[3])/255.0f);
            if(vs.size() == 5) cfg.sky_brightness = std::stof(vs[4]);
        }else if(vs[0] == "lens" || vs[0] == "lenssize" || vs[0] == "lens_size"){
            if(vs.size() != 2) throw ConfigFileException("Invalid lens size line.");
            float ls = std::stof(vs[1]);
            if(ls < 0) throw ConfigFileException("Lens size must be a poositive value.");
            cfg.lens_size = ls;
        }else if(vs[0] == "focus" || vs[0] == "focus_plane" || vs[0] == "focus_dist"){
            if(vs.size() != 2) throw ConfigFileException("Invalid focus plane line.");
            float fp = std::stof(vs[1]);
            if(fp < 0) throw ConfigFileException("Focus plane must be a poositive value.");
            cfg.focus_plane = fp;
        }else if(vs[0] == "bump_scale" || vs[0] == "bumpmap_scale" || vs[0] == "bump" || vs[0] == "bumpscale"){
            if(vs.size() != 2) throw ConfigFileException("Invalid bump scale config line.");
            float bs = std::stof(vs[1]);
            cfg.bumpmap_scale = bs;
        }else if(vs[0] == "clamp"){
            if(vs.size() != 2) throw ConfigFileException("Invalid clamp config line.");
            cfg.clamp = std::stof(vs[1]);
        }else if(vs[0] == "russian" || vs[0] == "roulette"){
            if(vs.size() != 2) throw ConfigFileException("Invalid russian roulette config line.");
            cfg.russian = std::stof(vs[1]);
        }else{
            std::cout << "WARNING: Unrecognized option `" << vs[0] << "` in the config file." << std::endl;

        }
    }while (file.good());

    return cfg;
}
