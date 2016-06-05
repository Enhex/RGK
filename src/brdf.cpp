#include "brdf.hpp"

#include "glm.hpp"
#include <glm/gtx/vector_angle.hpp>

#include "out.hpp"
#include "utils.hpp"
#include "LTC/ltc_beckmann.hpp"

std::tuple<glm::vec3, float, BRDF::BRDFSamplingType> BRDF::GetRay(glm::vec3 normal, glm::vec3, Random &rnd) const{
    glm::vec3 v = rnd.GetHSCosDir(normal);
    float p = glm::dot(normal,v)/glm::pi<float>();
    assert(glm::dot(normal, v) > -0.01f);
    return std::make_tuple(v,p,SAMPLING_COSINE);
}


float BRDFDiffuseUniform::PdfDiff() const{
    return 1.0f/glm::pi<float>();
}
float BRDFDiffuseUniform::PdfSpec(glm::vec3, glm::vec3, glm::vec3, bool) const{
    return 0.0f;
}
std::tuple<glm::vec3, float, BRDF::BRDFSamplingType> BRDFDiffuseUniform::GetRay(glm::vec3 normal, glm::vec3, Random &rnd) const{
    glm::vec3 v = rnd.GetHSUniformDir(normal);
    float p = 0.5f/glm::pi<float>();
    assert(glm::dot(normal, v) > 0.0f);
    return std::make_tuple(v,p,SAMPLING_UNIFORM);
}


float BRDFDiffuseCosine::PdfDiff() const{
    return 1.0f/glm::pi<float>();
}
float BRDFDiffuseCosine::PdfSpec(glm::vec3, glm::vec3, glm::vec3, bool) const{
    return 0.0f;
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

    //IFDEBUG std::cout << "N = " << N << std::endl;
    //IFDEBUG std::cout << "Vi = " << Vi << std::endl;
    //IFDEBUG std::cout << "Vh = " << Vh << std::endl;
    //IFDEBUG std::cout << "Vr = " << Vr << std::endl;

    float th_i = 0.5f*glm::pi<float>() - glm::angle(Vi, N);
    float th_r = 0.5f*glm::pi<float>() - glm::angle(Vr, N);
    float th_h = 0.5f*glm::pi<float>() - glm::angle(Vh, N);
    float beta = 0.5f*glm::angle(Vi, Vr);

    // Schlich approximation for Fresnel function
    float cb = 1.0f - glm::dot(Vi, Vh);
    float F = F0 + (1.0f - F0) * cb*cb*cb*cb*cb;

    //IFDEBUG std::cout << "cb = " << cb << std::endl;
    //IFDEBUG std::cout << "F0 = " << F0 << std::endl;

    // Beckman dist
    //float ce = glm::cos(th_h);
    //float te = glm::tan(th_h);
    float m2 = roughness * roughness;
    //float D = glm::exp(-1.0f * te*te / m2 )/(m2 * ce*ce*ce*ce);

    float NdotH = glm::dot(N, Vh);
    float r1 = 1.0f / ( 4.0f * m2 * NdotH * NdotH * NdotH * NdotH);
    float r2 = (NdotH * NdotH - 1.0) / (m2 * NdotH * NdotH);
    float D = r1 * glm::exp(r2);

    //IFDEBUG std::cout << "m2 = " << m2 << std::endl;
    //IFDEBUG std::cout << "te = " << te << std::endl;
    //IFDEBUG std::cout << "ce = " << ce << std::endl;

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

    //IFDEBUG std::cout << "F = " << F << std::endl;
    //IFDEBUG std::cout << "D = " << D << std::endl;
    //IFDEBUG std::cout << "G = " << G << std::endl;
    //IFDEBUG std::cout << "NdotV = " << NdotV << std::endl;
    //IFDEBUG std::cout << "NdotL = " << NdotL << std::endl;
    //IFDEBUG std::cout << "c = " << c << std::endl;

    return c / glm::pi<float>();
}

float BRDFLTCBeckmann::PdfDiff() const{
    return 1.0f/glm::pi<float>();
}
float BRDFLTCBeckmann::PdfSpec(glm::vec3 N, glm::vec3 Vi, glm::vec3 Vr, bool debug) const{
    //IFDEBUG std::cout << "Debugging beckman ltc" << std::endl;
    return LTC_BECKMANN::get_pdf(N, Vi, Vr, roughness, debug);
}
BRDFLTCBeckmann::BRDFLTCBeckmann(float phong_exp){
    // Converting specular exponent to roughness using Brian Karis' formula:
    roughness = glm::pow(2.0f / (2.0f + phong_exp), 0.25f);
    out::cout(3) << "Created new BRDF LTC Beckmann with roughness = " << roughness << std::endl;
}

std::tuple<glm::vec3, float, BRDF::BRDFSamplingType> BRDFLTCBeckmann::GetRay(glm::vec3 normal, glm::vec3 inc, Random &rnd) const{
    /*
    assert(glm::dot(normal, inc) > 0.0f);
    glm::vec3 v = rnd.GetHSCosZ();
    v = LTC_BECKMANN::get_random(normal, inc, roughness, v);
    assert(glm::dot(normal, v) > 0.0f);
    return std::make_tuple(v,1.0f,SAMPLING_BRDF);
    */
    return BRDF::GetRay(normal,inc,rnd);
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


*/
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
