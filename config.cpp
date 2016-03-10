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
    if(k==0) throw ConfigFileException("Invalid k value.");
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
    if(k <= 0.0f || k >= 100.0f) throw ConfigFileException("Invalid yview value.");
    cfg.yview = yview;

    // Now for the extra lines
    line = "";
    do{
        std::getline(file, line);
        line = Utils::Trim(line);
        vs = Utils::SplitString(line," ");
        if(vs.size() == 0) continue;
        if(vs[0][0] == '#') continue;
        if(vs[0] == "L"){
            // Light info
            if(vs.size() != 8) throw ConfigFileException("Invalid light line.");
            float l1 = std::stof(vs[1]), l2 = std::stof(vs[2]), l3 = std::stof(vs[3]);
            float c1 = std::stof(vs[4])/255, c2 = std::stof(vs[5])/255, c3 = std::stof(vs[6])/255;
            float i = std::stof(vs[7]);
            cfg.lights.push_back(Light{glm::vec3(l1,l2,l3), Color(c1,c2,c3), i});
        }else if(vs[0] == "multisample" || vs[0] == "ms"){
            if(vs.size() != 2) throw ConfigFileException("Invalid multisample line.");
            int ms = std::stoi(vs[1]);
            if(ms == 0) throw ConfigFileException("Invalid multisample value.");
            cfg.multisample = ms;
        }else{
            std::cout << "WARNING: Unrecognized option `" << vs[0] << "` in the config file." << std::endl;

        }
    }while (file.good());

    return cfg;
}
