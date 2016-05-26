#include "../global_config.hpp"
#include "../glm.hpp"

struct mat33
{
    operator glm::mat3() const
    {
        return glm::mat3(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
    }

    double m[9];
};

namespace LTC_BECKMANN{
    glm::mat3 get_floor(const float theta, const float alpha);
    float get_pdf(glm::vec3 N, glm::vec3 Vr, glm::vec3 Vi, float alpha, bool debug = false);
};
