#ifndef __PATH_TRACER_HPP__
#define __PATH_TRACER_HPP__

#include <set>
#include "tracer.hpp"
#include "random.hpp"

class PathTracer : public Tracer{
public:
    PathTracer(const Scene& scene,
               const Camera& camera,
               unsigned int xres,
               unsigned int yres,
               unsigned int multisample,
               unsigned int depth,
               Color sky_color,
               float sky_brightness,
               float clamp,
               float russian,
               float bumpmap_scale,
               std::set<const Material *> thinglass_names,
               Random rnd);

protected:
    Radiance RenderPixel(int x, int y, unsigned int & raycount, bool debug = false) override;

private:
    Radiance TracePath(const Ray& r, unsigned int& raycount, bool debug = false);

    struct PathPoint{
        enum Type{
            SCATTERED,
            REFLECTED,
            ENTERED,
            LEFT,
        };
        Type type;
        bool infinity = false;
        glm::vec3 pos;
        glm::vec3 lightN;
        glm::vec3 faceN;
        glm::vec3 Vr; // reflected direction (pointing towards previous path point)
        glm::vec3 Vi; // incoming  direction (pointing towards next path point)
        glm::vec2 texUV;
        const Material* mat;
        Color diffuse;
        Color specular;
        ThinglassIsections thinglass_isect;
        BRDF::BRDFSamplingType sampling_type;
        float russian_coefficient;
        // These take into account sampling, BRDF, color. Symmetric in both directions.
        Radiance transfer_coefficients;
    };

    std::vector<PathPoint> GeneratePath(Ray direction, unsigned int& raycount, unsigned int depth__, float russian__, bool debug = false) const;

    Radiance ApplyThinglass(Radiance input, const ThinglassIsections& isections, glm::vec3 ray_direction) const;

    Radiance sky_radiance;
    float clamp;
    float russian;
    unsigned int depth;
    std::set<const Material*> thinglass;
    mutable Random rnd;
};

#endif // __PATH_TRACER_HPP__
