#ifndef __BRDF_HPP__
#define __BRDF_HPP__

#include "glm.hpp"
#include "radiance.hpp"

// Arguments:
//  normal, Kd, Ks, incoming, reflected, specular exponent
typedef Radiance (*BRDF_fptr)(glm::vec3, Color, Color, glm::vec3, glm::vec3, float);

class BRDF{
public:
    static Radiance Diffuse(glm::vec3, Color, Color, glm::vec3, glm::vec3, float);
    static Radiance Phong  (glm::vec3, Color, Color, glm::vec3, glm::vec3, float exp);
};

void TestBRDF(BRDF_fptr);


#endif // __BRDF_HPP__
