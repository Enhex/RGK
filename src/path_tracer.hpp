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
               bool  force_fresnell,
               unsigned int reverse,
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
        // Point position
        glm::vec3 pos;
        // Normal vectors
        glm::vec3 lightN;
        glm::vec3 faceN;
        // reflected direction (pointing towards previous path point)
        glm::vec3 Vr;
        // incoming  direction (pointing towards next path point)
        glm::vec3 Vi;
        // Material properties at hitpoint
        const Material* mat;
        Color diffuse;
        Color specular;
        // Thinglass encountered on the way of the ray that generated this point
        ThinglassIsections thinglass_isect;
        // Currection for rusian roulette
        float russian_coefficient;
        // These take into account sampling, BRDF, color. Symmetric in both directions.
        Radiance transfer_coefficients;
        //
        Radiance light_from_source;
    };

    std::vector<PathPoint> GeneratePath(Ray direction, unsigned int& raycount, unsigned int depth__, float russian__, bool debug = false) const;

    Radiance ApplyThinglass(Radiance input, const ThinglassIsections& isections, glm::vec3 ray_direction) const;

    Radiance sky_radiance;
    float clamp;
    float russian;
    unsigned int depth;
    std::set<const Material*> thinglass;
    bool force_fresnell;
    unsigned int reverse;
    mutable Random rnd;
};

#endif // __PATH_TRACER_HPP__
