#ifndef __RAY_TRACER_HPP__
#define __RAY_TRACER_HPP__

#include "tracer.hpp"

#include "random.hpp"
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
              unsigned int depth,
              Color sky_color,
              float bumpmap_scale,
              Random rnd)
        : Tracer(scene, camera, lights, xres, yres, multisample, bumpmap_scale),
          depth(depth),
          sky_color(sky_color),
          shadow_cache(lights.size(), LRUBuffer<const Triangle*>(SHADOW_CACHE_SIZE)),
          rnd(rnd)
    {}

protected:
    Radiance RenderPixel(int x, int y, unsigned int & raycount, bool debug = false) override;

private:
    Color TraceRay(const Ray& r, unsigned int depth, unsigned int& raycount, bool debug = false);

    unsigned int depth;
    Color sky_color;

    mutable std::vector<LRUBuffer<const Triangle*>> shadow_cache;

    mutable Random rnd;
};

#endif // __RAY_TRACER_HPP__
