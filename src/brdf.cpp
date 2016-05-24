#include "brdf.hpp"

#include "glm.hpp"
#include <glm/gtx/vector_angle.hpp>

float BRDFDiffuseUniform::PdfDiff() const{
    return 1.0f/glm::pi<float>();
}
float BRDFDiffuseUniform::PdfSpec(glm::vec3, glm::vec3, glm::vec3) const{
    return 0.0f;
}
std::pair<glm::vec3, float> BRDFDiffuseUniform::GetRay(glm::vec3 normal, Random &rnd) const{
    glm::vec3 v = rnd.GetHSUniformDir(normal);
    float p = 0.5f/glm::pi<float>();
    assert(glm::dot(normal, v) > 0.0f);
    return {v,p};
}

float BRDFDiffuseCosine::PdfDiff() const{
    return 1.0f/glm::pi<float>();
}
float BRDFDiffuseCosine::PdfSpec(glm::vec3, glm::vec3, glm::vec3) const{
    return 0.0f;
}
std::pair<glm::vec3, float> BRDF::GetRay(glm::vec3 normal, Random &rnd) const{
    glm::vec3 v = rnd.GetHSCosDir(normal);
    float p = glm::dot(normal,v)/glm::pi<float>();
    assert(glm::dot(normal, v) > -0.01f);
    return {v,p};
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
float BRDFCookTorr::PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr) const{
    glm::vec3 Vh = glm::normalize(Vi + Vr);
    float th_i = 0.5f*glm::pi<float>() - glm::angle(Vi, N);
    float th_r = 0.5f*glm::pi<float>() - glm::angle(Vr, N);
    float th_h = 0.5f*glm::pi<float>() - glm::angle(Vh, N);
    float beta = 0.5f*glm::angle(Vi, Vr);

    // Schlich approximation for Fresnel function
    float cb = 1.0f - glm::cos(beta);
    float F = F0 + (1.0f - F0) * cb*cb*cb*cb*cb;

    // Beckman dist
    float ce = glm::cos(th_h);
    float te = glm::tan(th_h);
    float m2 = roughness * roughness;
    float D = glm::exp(-1.0f * te*te / m2 )/(m2 * ce*ce*ce*ce);

    // Shadow and masking factor
    float Gc = 2.0f * glm::cos(th_h) / glm::cos(beta);
    float G1 = Gc * glm::cos(th_i);
    float G2 = Gc * glm::cos(th_r);
    float G = glm::max(1.0f, glm::max(G1,G2));

    float c = F * D * G / (glm::cos(th_i) * glm::cos(th_r));

    return c / glm::pi<float>();

}

// TODO: Re-implement other BRDF functions!

/*
Radiance BRDF::Phong(glm::vec3 N, Color Kd, Color Ks, glm::vec3 Vi, glm::vec3 Vr, float exponent, float, float){
    // Ideal specular reflection direction
    glm::vec3 Vs = 2.0f * glm::dot(Vi, N) * N - Vi;

    float c = glm::max(0.0f, glm::cos( glm::angle(Vr,Vs) ) );
    c = glm::pow(c, exponent);
    Radiance diffuse  = Radiance(Kd) / glm::pi<float>();
    Radiance specular = Radiance(Ks) * c;
    return diffuse + specular;
}

Radiance BRDF::Phong2(glm::vec3 N, Color Kd, Color Ks, glm::vec3 Vi, glm::vec3 Vr, float exponent, float, float){
    // Ideal specular reflection direction
    glm::vec3 Vs = 2.0f * glm::dot(Vi, N) * N - Vi;

    float c = glm::max(0.0f, glm::dot(Vr,Vs));
    c = glm::pow(c, exponent);
    c /= glm::dot(Vi, N);

    Radiance diffuse  = Radiance(Kd) / glm::pi<float>();
    Radiance specular = Radiance(Ks) * c;
    return diffuse + specular;
}

Radiance BRDF::PhongEnergyConserving(glm::vec3 N, Color Kd, Color Ks, glm::vec3 Vi, glm::vec3 Vr, float exponent, float, float){
    // Ideal specular reflection direction
    glm::vec3 Vs = 2.0f * glm::dot(Vi, N) * N - Vi;

    float c = glm::max(0.0f, glm::dot(Vr,Vs));
    c = glm::pow(c, exponent);
    c /= glm::dot(Vi, N);

    float norm = (exponent + 2) / (2.0f * glm::pi<float>());

    Radiance diffuse  = Radiance(Kd) / glm::pi<float>();
    Radiance specular = Radiance(Ks) * norm * c;
    return diffuse + specular;
}
*/
