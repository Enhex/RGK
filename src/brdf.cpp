#include "brdf.hpp"

#include "glm.hpp"
#include <glm/gtx/vector_angle.hpp>
#include <glm/gtx/string_cast.hpp>

#include "out.hpp"
#include "utils.hpp"
#include "LTC/ltc.hpp"

std::tuple<glm::vec3, Radiance, BRDF::BRDFSamplingType> BRDF::GetRay(glm::vec3 normal, glm::vec3, Radiance, Radiance, Random &rnd, bool) const{
    glm::vec3 v = rnd.GetHSCosDir(normal);
    assert(glm::dot(normal, v) > -0.01f);
    return std::make_tuple(v,Radiance(1.0,1.0,1.0),SAMPLING_COSINE);
}

float BRDFDiffuseUniform::PdfDiff() const{
    return 1.0f/glm::pi<float>();
}
float BRDFDiffuseUniform::PdfSpec(glm::vec3, glm::vec3, glm::vec3, bool) const{
    return 0.0f;
}
std::tuple<glm::vec3, Radiance, BRDF::BRDFSamplingType> BRDFDiffuseUniform::GetRay(glm::vec3 normal, glm::vec3, Radiance, Radiance, Random &rnd, bool) const{
    glm::vec3 v = rnd.GetHSUniformDir(normal);
    assert(glm::dot(normal, v) >= 0.0f);
    return std::make_tuple(v,Radiance(1.0,1.0,1.0),SAMPLING_UNIFORM);
}

float BRDFDiffuseCosine::PdfDiff() const{
    return 1.0f/glm::pi<float>();
}
float BRDFDiffuseCosine::PdfSpec(glm::vec3, glm::vec3, glm::vec3, bool) const{
    return 0.0f;
}

float BRDFPhongEnergy::PdfDiff() const{
    return 1.0f/glm::pi<float>();
}
float BRDFPhongEnergy::PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr, bool) const{
    // Ideal specular reflection direction
    glm::vec3 Vs = 2.0f * glm::dot(Vi, N) * N - Vi;

    float c = glm::max(0.0f, glm::dot(Vr,Vs));
    c = glm::pow(c, exponent);
    c /= glm::dot(Vi, N);

    float norm = (exponent + 2) / (2.0f * glm::pi<float>());
    return norm * c;
}

BRDFCookTorr::BRDFCookTorr(float phong_exp, float ior){
    // Converting specular exponent to roughness using Brian Karis' formula:
    roughness = glm::pow(2.0f / (2.0f + phong_exp), 0.25f);
    // Fresnel approximation for perpendicular reflection
    float q = (1.0f - ior)/(1.0f + ior);
    F0 = q*q;
}
float BRDFCookTorr::PdfDiff() const{
    return 1.0f/glm::pi<float>();
}
float BRDFCookTorr::PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr, bool debug) const{
    (void)debug;
    glm::vec3 Vh = glm::normalize(Vi + Vr);
    //if(glm::length(Vi + Vr) < 0.001f) ...?

    float th_i = 0.5f*glm::pi<float>() - glm::angle(Vi, N);
    float th_r = 0.5f*glm::pi<float>() - glm::angle(Vr, N);
    float th_h = 0.5f*glm::pi<float>() - glm::angle(Vh, N);
    float beta = 0.5f*glm::angle(Vi, Vr);

    // Schlich approximation for Fresnel function
    float cb = 1.0f - glm::dot(Vi, Vh);
    float F = F0 + (1.0f - F0) * cb*cb*cb*cb*cb;

    // Beckman dist
    //float ce = glm::cos(th_h);
    //float te = glm::tan(th_h);
    float m2 = roughness * roughness;

    float NdotH = glm::dot(N, Vh);
    float r1 = 1.0f / ( 4.0f * m2 * NdotH * NdotH * NdotH * NdotH);
    float r2 = (NdotH * NdotH - 1.0) / (m2 * NdotH * NdotH);
    float D = r1 * glm::exp(r2);

    // Shadow and masking factor
    float Gc = 2.0f * glm::cos(th_h) / glm::cos(beta);
    float G1 = Gc * glm::cos(th_i);
    float G2 = Gc * glm::cos(th_r);
    float G = glm::max(1.0f, glm::max(G1,G2));

    float NdotV = glm::dot(N,Vi);
    if(NdotV < 0.001f) return 0.0f;
    float VdotH = glm::dot(Vi,Vh);
    float NdotL = NdotV;
    float NH2 = 2.0f * NdotH;
    float g1 = (NH2 * NdotV) / VdotH;
    float g2 = (NH2 * NdotL) / VdotH;
    G = glm::min(1.0f, glm::min(g1, g2));

    float c = F * D * G / (NdotV * NdotL * 3.14);

    return c / glm::pi<float>();
}


BRDFLTCBeckmann::BRDFLTCBeckmann(float phong_exp){
    // Converting specular exponent to roughness using Brian Karis' formula:
    roughness = glm::pow(2.0f / (2.0f + phong_exp), 0.5f);
    out::cout(4) << "Created new BRDF LTC Beckmann with roughness = " << roughness << std::endl;
}
float BRDFLTCBeckmann::PdfDiff() const{
    return 1.0f/glm::pi<float>();
}
float BRDFLTCBeckmann::PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr, bool debug) const{
    return LTC::GetPDF(LTC::Beckmann, N, Vi, Vr, roughness, debug);
}
std::tuple<glm::vec3, Radiance, BRDF::BRDFSamplingType> BRDFLTCBeckmann::GetRay(glm::vec3 normal, glm::vec3 inc, Radiance diffuse, Radiance specular, Random &rnd, bool debug) const{
    assert(glm::dot(normal, inc) > 0.0f);
    float diffuse_power = diffuse.r + diffuse.g + diffuse.b; // Integral over diffuse spectrum...
    float specular_power = specular.r + specular.g + specular.b; // Integral over specular spectrum...
    if(rnd.GetFloat1D() * (diffuse_power + specular_power) < diffuse_power){
        // Diffuse ray
        auto res = BRDF::GetRay(normal, inc, diffuse, specular, rnd);
        std::get<1>(res) = Radiance(diffuse);
        return res;
    }else{
        glm::vec3 v;
        v = rnd.GetHSCosZ();
        v = LTC::GetRandom(LTC::Beckmann, normal, inc, roughness, v, debug);
        return std::make_tuple(v,Radiance(specular),SAMPLING_BRDF);
    }
}

BRDFLTCGGX::BRDFLTCGGX(float phong_exp){
    // Converting specular exponent to roughness using Brian Karis' formula:
    roughness = glm::pow(2.0f / (2.0f + phong_exp), 0.5f);
    out::cout(4) << "Created new BRDF LTC GGX with roughness = " << roughness << std::endl;
}
float BRDFLTCGGX::PdfDiff() const{
    return 1.0f/glm::pi<float>();
}
float BRDFLTCGGX::PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr, bool debug) const{
    return LTC::GetPDF(LTC::GGX, N, Vi, Vr, roughness, debug);
}
std::tuple<glm::vec3, Radiance, BRDF::BRDFSamplingType> BRDFLTCGGX::GetRay(glm::vec3 normal, glm::vec3 inc, Radiance diffuse, Radiance specular, Random &rnd, bool debug) const{
    qassert_directed(normal, inc);
    float diffuse_power = diffuse.r + diffuse.g + diffuse.b; // Integral over diffuse spectrum...
    float specular_power = specular.r + specular.g + specular.b; // Integral over specular spectrum...
    if(rnd.GetFloat1D() * (diffuse_power + specular_power) < diffuse_power){
        // Diffuse ray
        auto res = BRDF::GetRay(normal, inc, diffuse, specular, rnd);
        std::get<1>(res) = Radiance(diffuse);
        return res;
    }else{
        glm::vec3 v;
        v = rnd.GetHSCosZ();
        v = LTC::GetRandom(LTC::GGX, normal, inc, roughness, v, debug);
        return std::make_tuple(v,Radiance(specular),SAMPLING_BRDF);
    }
}
