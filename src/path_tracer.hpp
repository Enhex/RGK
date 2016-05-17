#ifndef __PATH_TRACER_HPP__
#define __PATH_TRACER_HPP__

#include "tracer.hpp"

class PathTracer : public Tracer{
public:
    PathTracer(const Scene& scene,
               const Camera& camera,
               const std::vector<Light>& lights,
               unsigned int xres,
               unsigned int yres,
               unsigned int multisample,
               unsigned int depth,
               Color sky_color,
               float sky_brightness,
               float clamp,
               float russian,
               float bumpmap_scale)
        : Tracer(scene, camera, lights, xres, yres, multisample, bumpmap_scale),
          clamp(clamp),
          russian(russian),
          depth(depth)
    {
        sky_radiance = Radiance(sky_color) * sky_brightness;
    }

protected:
    Radiance RenderPixel(int x, int y, unsigned int & raycount, bool debug = false) override;

private:
    Radiance TracePath(const Ray& r, unsigned int& raycount, bool debug = false);

    Radiance sky_radiance;
    float clamp;
    float russian;
    unsigned int depth;
};

#endif // __PATH_TRACER_HPP__
