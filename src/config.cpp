#include "config.hpp"

#include <fstream>

#include "utils.hpp"
#include "scene.hpp"
#include "out.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>

#define NEXT_LINE()                                                                  \
    do{ std::getline(file, line);                                                    \
        line = Utils::Trim(line);                                                    \
        if(!file.good()) throw ConfigFileException("Config file ends prematurely."); \
    }while(0)

std::shared_ptr<ConfigRTC> ConfigRTC::CreateFromFile(std::string path){
    auto cfgptr = std::shared_ptr<ConfigRTC>(new ConfigRTC());
    ConfigRTC& cfg = *cfgptr;
    cfg.config_file_path = path;

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
    cfg.camera_position = glm::vec3(VPx, VPy, VPz);

    NEXT_LINE(); // LA
    vs = Utils::SplitString(line," ");
    if(vs.size()!=3) throw ConfigFileException("Invalid LA format.");
    float LAx = std::stof(vs[0]), LAy = std::stof(vs[1]), LAz = std::stof(vs[2]);
    // TODO: What it LA - VP = 0 ?
    cfg.camera_lookat = glm::vec3(LAx, LAy, LAz);

    NEXT_LINE(); // UP
    vs = Utils::SplitString(line," ");
    if(vs.size()!=3) throw ConfigFileException("Invalid UP format.");
    float UPx = std::stof(vs[0]), UPy = std::stof(vs[1]), UPz = std::stof(vs[2]);
    cfg.camera_upvector = glm::vec3(UPx, UPy, UPz);

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
            Light l;
            l.type = Light::FULL_SPHERE;
            l.pos = glm::vec3(l1,l2,l3);
            l.color = Color(c1,c2,c3);
            l.size = s;
            l.intensity = i;
            cfg.lights.push_back(l);
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
        }else if(vs[0] == "rounds"){
            if(vs.size() != 2) throw ConfigFileException("Invalid rounds config line.");
            cfg.rounds = std::stoi(vs[1]);
        }else if(vs[0] == "reverse"){
            if(vs.size() != 2) throw ConfigFileException("Invalid reverse config line.");
            cfg.reverse = std::stoi(vs[1]);
        }else if(vs[0] == "brdf"){
            if(vs.size() != 2) throw ConfigFileException("Invalid brdf config line.");
            if(vs[1] == "cooktorr"){
                cfg.brdf = "cooktorr";
            }else if(vs[1] == "phong"){
                cfg.brdf = "phong";
            }else if(vs[1] == "phong2"){
                cfg.brdf = "phong2";
            }else if(vs[1] == "phongenergy"){
                cfg.brdf = "phongenergy";
            }else if(vs[1] == "diffuse"){
                cfg.brdf = "diffusecosine";
            }else if(vs[1] == "diffuseuniform"){
                cfg.brdf = "diffuseuniform";
            }else if(vs[1] == "ltc_beckmann"){
                cfg.brdf = "ltc_beckmann";
            }else if(vs[1] == "ltc_ggx"){
                cfg.brdf = "ltc_ggx";
            }else{
                throw ConfigFileException("Unknown BRDF type: " + vs[1]);
            }
        }else if(vs[0] == "thinglass"){
            if(vs.size() != 2) throw ConfigFileException("Invalid thinglass config line.");
            cfg.thinglass.push_back(vs[1]);
        }else if(vs[0] == "force_fresnell"){
            if(vs.size() != 2) throw ConfigFileException("Invalid force_fresnell config line.");
            cfg.force_fresnell = (std::stoi(vs[1]) == 1);
        }else{
            std::cout << "WARNING: Unrecognized option `" << vs[0] << "` in the config file." << std::endl;
        }
    }while (file.good());

    return cfgptr;
}


Camera ConfigRTC::GetCamera(float rotation) const{
    glm::vec3 p = camera_lookat - camera_position;
    p = glm::rotate(p, rotation * 2.0f * glm::pi<float>(), camera_upvector);
    glm::vec3 pos = camera_lookat - p;
    return Camera(pos,
                  camera_lookat,
                  camera_upvector,
                  yview,
                  yview*xres/yres,
                  xres,
                  yres,
                  focus_plane,
                  lens_size
                  );
}

void ConfigRTC::InstallLights(Scene& scene) const{
    for(const Light& l : lights)
        scene.AddPointLight(l);
}

static const aiScene* loadAssimpScene(Assimp::Importer& importer, std::string modelfile){
    out::cout(2) << "Loading scene from \"" << modelfile << "\"..."  << std::endl;
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE, nullptr);
    const aiScene* scene = importer.ReadFile(modelfile,
                                             aiProcess_Triangulate |
                                             //aiProcess_TransformUVCoords |

                                             // Neither of these work correctly.
                                             aiProcess_GenNormals |
                                             //aiProcess_GenSmoothNormals |

                                             aiProcess_JoinIdenticalVertices |

                                             //aiProcess_RemoveRedundantMaterials |

                                             aiProcess_GenUVCoords |
                                             //aiProcess_SortByPType |
                                             aiProcess_FindDegenerates |
                                             // DO NOT ENABLE THIS CLEARLY BUGGED SEE SIBENIK MODEL aiProcess_FindInvalidData |
                                             //aiProcess_ValidateDataStructure |
                                             0 );

    if(!scene) throw ConfigFileException("Assimp failed to load scene \"" + modelfile + "\": " + importer.GetErrorString());

    // Calculating tangents is requested AFTER the scene is
    // loaded. Otherwise this step runs before normals are calculated
    // for vertices that are missing them.
    scene = importer.ApplyPostProcessing(aiProcess_CalcTangentSpace);

    out::cout(2) << "Loaded scene with " << scene->mNumMeshes << " meshes and " <<
        scene->mNumMaterials << " materials." << std::endl;

    return scene;
}

void ConfigRTC::InstallScene(Scene& s) const{
    std::string configdir = Utils::GetDir(config_file_path);
    std::string modelfile = configdir + "/" + model_file;
    std::string modeldir  = Utils::GetDir(modelfile);
    if(!Utils::GetFileExists(modelfile))
        throw ConfigFileException("Unable to find model file \"" + modelfile + "\"");

    // Load the model with assimp
    Assimp::Importer importer;
    const aiScene* scene = loadAssimpScene(importer, modelfile);

    s.LoadAiSceneMaterials(scene, brdf, modeldir + "/");
    s.LoadAiSceneMeshes(scene);
}

std::pair<Color, float> ConfigRTC::GetSky() const{
    return std::make_pair(sky_color, sky_brightness);
}


// ----- JSON ----

bool JSONArrayToVec3(Json::Value v, glm::vec3& out){
    if(!v.isArray() || v.size() != 3) return false;
    const auto& x = v[0]; if(!x.isNumeric()) return false; out.x = x.asFloat();
    const auto& y = v[1]; if(!y.isNumeric()) return false; out.y = y.asFloat();
    const auto& z = v[2]; if(!z.isNumeric()) return false; out.z = z.asFloat();
    return true;
}

static inline std::string getRequiredString(const Json::Value& node, std::string key) {
    if(!node.isMember(key)) throw ConfigFileException("Value \"" + key +"\" is missing.");
    if(!node[key].isString()) throw ConfigFileException("Value \""+ key + "\" must be a string.");
    return node[key].asString();
}
static inline int getRequiredInt(const Json::Value& node, std::string key) {
    if(!node.isMember(key)) throw ConfigFileException("Value \"" + key +"\" is missing.");
    if(!node[key].isNumeric()) throw ConfigFileException("Value \""+ key + "\" must be a number.");
    return node[key].asInt();
}
static inline float getRequiredFloat(const Json::Value& node, std::string key) {
    if(!node.isMember(key)) throw ConfigFileException("Value \"" + key +"\" is missing.");
    if(!node[key].isNumeric()) throw ConfigFileException("Value \""+ key + "\" must be a number.");
    return node[key].asFloat();
}
static inline glm::vec3 getRequiredVec3(const Json::Value& node, std::string key) {
    if(!node.isMember(key)) throw ConfigFileException("Value \"" + key +"\" is missing.");
    glm::vec3 res;
    if(!JSONArrayToVec3(node[key], res))
        throw ConfigFileException("Value \"" + key + "\" must be an array of 3 numbers.");
    return res;
}
static inline std::string getOptionalString(const Json::Value& node, std::string key, std::string def) {
    if(node.isMember(key) && !node[key].isString()) throw ConfigFileException("Value \""+ key + "\" must be a string.");
    return node.get(key, def).asString();
}
static inline int getOptionalInt(const Json::Value& node, std::string key, int def) {
    if(node.isMember(key) && !node[key].isNumeric()) throw ConfigFileException("Value \""+ key + "\" must be a number.");
    return node.get(key, def).asInt();
}
static inline float getOptionalFloat(const Json::Value& node, std::string key, float def) {
    if(node.isMember(key) && !node[key].isNumeric()) throw ConfigFileException("Value \""+ key + "\" must be a number.");
    return node.get(key, def).asFloat();
}
static inline bool getOptionalBool(const Json::Value& node, std::string key, bool def) {
    if(node.isMember(key) && !node[key].isBool()) throw ConfigFileException("Value \""+ key + "\" must be a bool.");
    return node.get(key, def).asBool();
}
static inline glm::vec3 getOptionalVec3(const Json::Value& node, std::string key, glm::vec3 def) {
    if(!node.isMember(key)) return def;
    glm::vec3 res;
    if(!JSONArrayToVec3(node[key], res))
        throw ConfigFileException("Value \"" + key + "\" must be an array of 3 numbers.");
    return res;
}

std::shared_ptr<ConfigJSON> ConfigJSON::CreateFromFile(std::string path){
    auto cfgptr = std::shared_ptr<ConfigJSON>(new ConfigJSON());
    ConfigJSON& cfg = *cfgptr;
    cfg.config_file_path = path;
    Json::Value& root = cfg.root;

    Json::Reader reader;

    std::ifstream file(path, std::ios::in);
    if(!file.good()) throw ConfigFileException("Failed to open file: " + path);

    reader.parse(file, root, false);
    if(!reader.good()) throw ConfigFileException("Failed to parse JSON contents: " + reader.getFormattedErrorMessages());

    cfg.output_file = getRequiredString(root,"output-file");

    cfg.xres = getRequiredInt(root,"output-width");
    cfg.yres = getRequiredInt(root,"output-height");

    cfg.recursion_level = getOptionalInt(root, "recursion-max", 1);
    cfg.rounds = getOptionalInt(root, "rounds", 1);
    cfg.multisample = getOptionalInt(root, "multisample", 1);
    cfg.clamp = getOptionalFloat(root, "clamp", 10000000.0f);
    cfg.bumpmap_scale = getOptionalFloat(root, "bumpscale", 1.0f);
    cfg.russian = getOptionalFloat(root, "russian", -1.0f);
    cfg.reverse = getOptionalInt(root, "reverse", 0);
    cfg.brdf = getOptionalString(root, "brdf", "cooktorr");
    cfg.force_fresnell = getOptionalBool(root, "force-fresnell", false);

    if(root.isMember("thinglass")){
        auto thinglass = root["thinglass"];
        if(!thinglass.isArray()) throw ConfigFileException("Value \"thinglass\" must be an array of strings");
        for(unsigned int i = 0; i < thinglass.size(); i++){
            auto t = thinglass[i];
            if(!t.isString()) throw ConfigFileException("Value \"thinglass\" must be an array of strings");
            cfg.thinglass.push_back(t.asString());
        }
    }

    return cfgptr;
}

Camera ConfigJSON::GetCamera(float rotation) const{
    if(!root.isMember("camera")) throw ConfigFileException("Value \"camera\" is missing.");
    auto camera = root["camera"];
    if(!camera.isObject()) throw ConfigFileException("Value \"camera\" is not a dictionary.");

    glm::vec3 camera_position = getRequiredVec3(camera, "position");
    glm::vec3 camera_lookat   = getRequiredVec3(camera, "lookat"  );
    glm::vec3 camera_upvector = getOptionalVec3(camera, "upvector", glm::vec3(0.0f, 1.0f, 0.0f));

    float yview = getRequiredFloat(camera, "focal");

    float focus_plane = getOptionalFloat(camera, "focus-plane", 1.0f);
    float lens_size   = getOptionalFloat(camera, "lens-size"  , 0.0f);

    glm::vec3 p = camera_lookat - camera_position;
    p = glm::rotate(p, rotation * 2.0f * glm::pi<float>(), camera_upvector);
    glm::vec3 pos = camera_lookat - p;
    return Camera(pos,
                  camera_lookat,
                  camera_upvector,
                  yview,
                  yview*xres/yres,
                  xres,
                  yres,
                  focus_plane,
                  lens_size
                  );
}

void ConfigJSON::InstallLights(Scene &scene) const{
    if(!root.isMember("lights")) return; // no lights!
    auto lights = root["lights"];
    if(!lights.isArray()) throw ConfigFileException("Value \"lights\" must be an array.");
    for(unsigned int i = 0; i < lights.size(); i++){
        auto light = lights[i];
        Light l;
        l.pos = getRequiredVec3(light, "position");
        l.color = getOptionalVec3(light, "color", glm::vec3(255.0f, 255.0f, 255.0f))/255.0f;
        l.intensity = getRequiredFloat(light, "intensity");
        l.size = getOptionalFloat(light, "size", 0.0f);
        scene.AddPointLight(l);
    }
}

std::pair<Color, float> ConfigJSON::GetSky() const{
    if(!root.isMember("sky")) return std::make_pair(Color(0.0f, 0.0f, 0.0f), 0.0f);
    auto sky = root["sky"];
    if(!sky.isObject()) throw ConfigFileException("Value \"sky\" must be a dictionary.");

    Color sky_color = getRequiredVec3(sky, "color")/255.0f;
    float sky_intensity = getRequiredFloat(sky, "intensity");
    return std::make_pair(sky_color, sky_intensity);
}


void ConfigJSON::InstallScene(Scene& s) const{
    std::string configdir = Utils::GetDir(config_file_path);
    if(root.isMember("model-file") && root.isMember("scene"))
        throw ConfigFileException("The input file may not contain both \"model-file\" key and \"scene\" key, maximum one of these is allowed.");
    if(root.isMember("model-file")){
        std::string modelfile = configdir + "/" + root["model-file"].asString();
        std::string modeldir  = Utils::GetDir(modelfile);
        if(!Utils::GetFileExists(modelfile))
            throw ConfigFileException("Unable to find model file \"" + modelfile + "\"");

        // Load the model with assimp
        Assimp::Importer importer;
        const aiScene* scene = loadAssimpScene(importer, modelfile);

        s.LoadAiSceneMaterials(scene, brdf, modeldir + "/");
        s.LoadAiSceneMeshes(scene);
    }else if(root.isMember("scene")){
        auto scene_node = root["scene"];
        if(!scene_node.isArray()) throw ConfigFileException("The value of \"scene\" key must be an array of objects");
        for(unsigned int i = 0; i < scene_node.size(); i++){
            auto object = scene_node[i];


            std::string model_file = getRequiredString(object, "file");
            bool import_materials = getOptionalBool(object, "import-materials", false);

            std::string modelfile = configdir + "/" + model_file;
            std::string modeldir  = Utils::GetDir(modelfile);
            if(!Utils::GetFileExists(modelfile))
                throw ConfigFileException("Unable to find model file \"" + modelfile + "\"");

            // Load the model with assimp
            Assimp::Importer importer;
            const aiScene* scene = loadAssimpScene(importer, modelfile);

            if(import_materials)
                s.LoadAiSceneMaterials(scene, brdf, modeldir + "/");
            s.LoadAiSceneMeshes(scene);
        }
    }else{
        throw ConfigFileException("The input file contains neither \"scene\" nor \"model-file\" key.");
    }
}
