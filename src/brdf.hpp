#ifndef __BRDF_HPP__
#define __BRDF_HPP__

#include "glm.hpp"
#include "radiance.hpp"
#include "random.hpp"

class BRDF{
public:
    enum BRDFType{
        Diffuse,
        Phong,
        Phong2,
        PhongEnergyConserving,
        CookTorr
    };
    virtual float PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr) const = 0;
    virtual float PdfDiff() const = 0;
    Radiance Apply(Color kD, Color kS, glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr) const{
        return Radiance(kD)*PdfDiff() + Radiance(kS)*PdfSpec(N,Vi,Vr);
    }
    // Default is cosine sampling
    virtual std::pair<glm::vec3, float> GetRay(glm::vec3 normal, Random& rnd) const;
};

class BRDFDiffuseUniform : public BRDF{
public:
    virtual float PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr) const override;
    virtual float PdfDiff() const override;
    virtual std::pair<glm::vec3, float> GetRay(glm::vec3 normal, Random& rnd) const override;
};

class BRDFDiffuseCosine : public BRDF{
public:
    virtual float PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr) const override;
    virtual float PdfDiff() const override;
};

class BRDFCookTorr : public BRDF{
public:
    BRDFCookTorr(float phong_exponent, float ior);
    virtual float PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr) const override;
    virtual float PdfDiff() const override;
private:
    float roughness;
    float F0;
};

#endif // __BRDF_HPP__
