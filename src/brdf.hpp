#ifndef __BRDF_HPP__
#define __BRDF_HPP__

#include "glm.hpp"
#include "radiance.hpp"
#include "random.hpp"
#include "global_config.hpp"
#include <iostream>
#include <tuple>

class BRDF{
public:
    enum BRDFType{
        Diffuse,
        Phong,
        Phong2,
        PhongEnergyConserving,
        CookTorr
    };
    virtual float PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr, bool debug = false) const = 0;
    virtual float PdfDiff() const = 0;
    Radiance Apply(Color kD, Color kS, glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr, bool debug = false) const{
        float d = PdfDiff();
        float s = PdfSpec(N,Vi,Vr, debug);
        IFDEBUG std::cout << "d = " << d << ", s = " << s << std::endl;
        return Radiance(kD)*d + Radiance(kS)*s;
    }
    // Default is cosine sampling
    enum BRDFSamplingType{
        SAMPLING_UNIFORM,
        SAMPLING_COSINE,
        SAMPLING_BRDF
    };
    virtual std::tuple<glm::vec3, float, BRDFSamplingType> GetRay(glm::vec3 normal, Random& rnd) const;
};

class BRDFDiffuseUniform : public BRDF{
public:
    virtual float PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr, bool debug = false) const override;
    virtual float PdfDiff() const override;
    virtual std::tuple<glm::vec3, float, BRDFSamplingType> GetRay(glm::vec3 normal, Random& rnd) const override;
};

class BRDFDiffuseCosine : public BRDF{
public:
    virtual float PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr, bool debug = false) const override;
    virtual float PdfDiff() const override;
};

class BRDFCookTorr : public BRDF{
public:
    BRDFCookTorr(float phong_exponent, float ior);
    virtual float PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr, bool debug = false) const override;
    virtual float PdfDiff() const override;
private:
    float roughness;
    float F0;
};

class BRDFLTCBeckmann : public BRDF{
public:
    BRDFLTCBeckmann(float phong_exponent);
    virtual float PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr, bool debug = false) const override;
    virtual float PdfDiff() const override;
private:
    float roughness;
};

#endif // __BRDF_HPP__
