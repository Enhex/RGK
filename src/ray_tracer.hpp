#ifndef __RAY_TRACER_HPP__
#define __RAY_TRACER_HPP__

#include "tracer.hpp"

#include "LRU.hpp"
#define SHADOW_CACHE_SIZE 5

class RayTracer : public Tracer{
public:
    RayTracer(const Scene& scene,
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
          sky_color(sky_color),
          shadow_cache(lights.size(), LRUBuffer<const Triangle*>(SHADOW_CACHE_SIZE))
    {}

    Radiance RenderPixel(int x, int y, unsigned int & raycount, bool debug = false) override;

private:
    Color TraceRay(const Ray& r, unsigned int depth, unsigned int& raycount, bool debug = false);

    unsigned int depth;
    Color sky_color;

    mutable std::vector<LRUBuffer<const Triangle*>> shadow_cache;
};

#endif // __RAY_TRACER_HPP__
