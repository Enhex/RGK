#ifndef __BXDF_HPP__
#define __BXDF_HPP__

#include <memory>

#include "../../external/json/json.h"

#include "../texture.hpp"
#include "../LTC/ltc.hpp"
#include "../random_utils.hpp"
#include "../jsonutils.hpp"
#include "../config.hpp"

class BxDF;
class Scene;
class aiMaterial;

class Material{
public:
    Material();
    std::string name;

    Radiance emission;
    std::shared_ptr<ReadableTexture> bumpmap;

    std::unique_ptr<BxDF> bxdf;

    bool is_thinglass = false;

    void LoadFromJson(Json::Value& node, Scene& scene, std::string texturedir);
    void LoadFromAiMaterial(const aiMaterial* mat, Scene& scene, std::string texturedir);
};

#define BxDFUpVector glm::vec3(0.0f, 0.0f, 1.0f)

class BxDF{
public:
    virtual Spectrum value(glm::vec3 Vi, glm::vec3 Vr, glm::vec2 texUV, bool debug = false) const = 0;
    virtual std::pair<glm::vec3, Spectrum> sample(glm::vec3 Vi, glm::vec2 texUV, glm::vec2 sample, bool debug = false) const = 0;
    virtual void LoadFromJson(Json::Value&, Scene&, std::string){};
};



class BxDFDiffuse : public BxDF{
public:
    virtual Spectrum value(glm::vec3 Vi, glm::vec3 Vr, glm::vec2 texUV, bool debug = false) const override;
    virtual std::pair<glm::vec3, Spectrum> sample(glm::vec3 Vi, glm::vec2 texUV, glm::vec2 sample, bool debug = false) const override;

    void LoadFromJson(Json::Value& node, Scene& scene, std::string texturedir) override;
    std::shared_ptr<ReadableTexture> diffuse = std::make_shared<EmptyTexture>();
};

class BxDFTransparent : public BxDF{
public:
    virtual Spectrum value(glm::vec3 Vi, glm::vec3 Vr, glm::vec2 texUV, bool debug = false) const override;
    virtual std::pair<glm::vec3, Spectrum> sample(glm::vec3 Vi, glm::vec2 texUV, glm::vec2 sample, bool debug = false) const override;
    float ior_outside;
    float ior_inside;

    void LoadFromJson(Json::Value& node, Scene& scene, std::string texturedir) override;
};

class BxDFMirror : public BxDF{
public:
    virtual Spectrum value(glm::vec3 Vi, glm::vec3 Vr, glm::vec2 texUV, bool debug = false) const override;
    virtual std::pair<glm::vec3, Spectrum> sample(glm::vec3 Vi, glm::vec2 texUV, glm::vec2 sample, bool debug = false) const override;

    std::shared_ptr<ReadableTexture> color = std::make_shared<EmptyTexture>();
    void LoadFromJson(Json::Value& node, Scene& scene, std::string texturedir) override;
};

class BxDFMix : public BxDF{
public:
    virtual Spectrum value(glm::vec3 Vi, glm::vec3 Vr, glm::vec2 texUV, bool debug = false) const override;
    virtual std::pair<glm::vec3, Spectrum> sample(glm::vec3 Vi, glm::vec2 texUV, glm::vec2 sample, bool debug = false) const override;

    void LoadFromJson(Json::Value& node, Scene& scene, std::string texturedir) override;
    std::shared_ptr<const Material> m1;
    std::shared_ptr<const Material> m2;
    float amt1;
};

class BxDFLTCBase : public BxDF{
public:
    float roughness;
    std::shared_ptr<ReadableTexture> color = std::make_shared<EmptyTexture>();

    void LoadFromJson(Json::Value& node, Scene& scene, std::string texturedir) override;
};

template <const LTCdef& ltc>
class BxDFLTC : public BxDFLTCBase{
public:
    virtual Spectrum value(glm::vec3 Vi, glm::vec3 Vr, glm::vec2 texUV, bool debug = false) const override{
        if(Vi.z <= 0 || Vr.z <= 0) return Spectrum(0);
        return color->GetSpectrum(texUV) *
            LTC::GetPDF(ltc, BxDFUpVector, Vi, Vr, roughness, debug);
    }
    virtual std::pair<glm::vec3, Spectrum> sample(glm::vec3 Vi, glm::vec2 texUV, glm::vec2 sample, bool debug = false) const override{
        glm::vec3 v = RandomUtils::Sample2DToHemisphereCosineZ(sample);
        qassert_false(std::isnan(v.x));
        v = LTC::GetRandom(ltc, BxDFUpVector, Vi, roughness, v, debug);
        if(v.z <= 0) return {v, Spectrum(0)};
        return std::make_pair(v,color->GetSpectrum(texUV));
    }
};

#endif // __BXDF_HPP__
