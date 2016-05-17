#include "brdf.hpp"

#include <glm/gtx/vector_angle.hpp>

Radiance BRDF::Diffuse(glm::vec3, Color Kd, Color, glm::vec3, glm::vec3, float){
    return Radiance(Kd) / glm::pi<float>();
}

Radiance BRDF::Phong(glm::vec3 N, Color Kd, Color Ks, glm::vec3 Vi, glm::vec3 Vr, float exponent){
    // Ideal specular reflection direction
    glm::vec3 Vs = 2.0f * glm::dot(Vi, N) * N - Vi;

    // BRDF
    float c = glm::max(0.0f, glm::cos( glm::angle(Vr,Vs) ) );
    c = glm::pow(c, exponent);
    Radiance diffuse  = Radiance(Kd) / glm::pi<float>();
    Radiance specular = Radiance(Ks) * c;
    return diffuse + specular;
}
