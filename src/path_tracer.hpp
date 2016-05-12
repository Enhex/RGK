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
              unsigned int depth = 1,
              Color sky_color = Color(0.0,0.0,0.0),
              float bumpmap_scale = 10.0)
        : Tracer(scene, camera, lights, xres, yres, multisample, bumpmap_scale),
          depth(depth),
          sky_color(sky_color)
    {}

protected:
    Radiance RenderPixel(int x, int y, unsigned int & raycount, bool debug = false) override;

private:
    Color TraceRay(const Ray& r, unsigned int depth, unsigned int& raycount, bool debug = false);

    unsigned int depth;
    Color sky_color;
};

#endif // __PATH_TRACER_HPP__
