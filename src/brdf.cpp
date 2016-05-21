#include "brdf.hpp"

#include "glm.hpp"
#include <glm/gtx/vector_angle.hpp>

Radiance BRDF::Diffuse(glm::vec3, Color Kd, Color, glm::vec3, glm::vec3, float, float, float){
    return Radiance(Kd) / glm::pi<float>();
}

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

Radiance BRDF::CookTorr(glm::vec3 N, Color Kd, Color Ks, glm::vec3 Vi, glm::vec3 Vr, float exponent, float n1, float n2){
    glm::vec3 Vh = glm::normalize(Vi + Vr);
    float th_i = 0.5f*glm::pi<float>() - glm::angle(Vi, N);
    float th_r = 0.5f*glm::pi<float>() - glm::angle(Vr, N);
    float th_h = 0.5f*glm::pi<float>() - glm::angle(Vh, N);
    float beta = 0.5f*glm::angle(Vi, Vr);

    // Fresnel approximation for perpendicular reflection
    float q = (n1 - n2)/(n1 + n2);
    float F0 = q*q;

    // Schlich approximation for Fresnel function
    float cb = 1.0f - glm::cos(beta);
    float F = F0 + (1.0f - F0) * cb*cb*cb*cb*cb;

    // Converting specular to roughness using Brian Karis' formula:
    float m = glm::pow(2.0f / (2.0f + exponent), 0.25f);

    // Beckman dist
    float ce = glm::cos(th_h);
    float te = glm::tan(th_h);
    float m2 = m*m;
    float D = glm::exp(-1.0f * te*te / m2 )/(m2 * ce*ce*ce*ce);

    // Shadow and masking factor
    float Gc = 2.0f * glm::cos(th_h) / glm::cos(beta);
    float G1 = Gc * glm::cos(th_i);
    float G2 = Gc * glm::cos(th_r);
    float G = glm::max(1.0f, glm::max(G1,G2));

    float c = F * D * G / (glm::cos(th_i) * glm::cos(th_r));

    Radiance diffuse  = Radiance(Kd) / glm::pi<float>();
    Radiance specular = Radiance(Ks) * c / glm::pi<float>();
    return diffuse + specular;
}
