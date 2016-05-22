#include "tracer.hpp"
#include "texture.hpp"
#include "global_config.hpp"

void Tracer::Render(const RenderTask& task, EXRTexture* output, std::atomic<int>& pixel_count, std::atomic<unsigned int>& ray_count){
    unsigned int pxdone = 0, raysdone = 0;
    for(unsigned int y = task.yrange_start; y < task.yrange_end; y++){
        for(unsigned int x = task.xrange_start; x < task.xrange_end; x++){
            bool d = false;
#if ENABLE_DEBUG
            if(debug_trace && x == debug_x && y == debug_y) d = true;
#endif
            Radiance px = RenderPixel(x, y, raysdone, d);

            output->AddPixel(x, y, px);
            pxdone++;
            if(pxdone % 100 == 0){
                pixel_count += 100;
                pxdone = 0;
            }
        }
    }
    pixel_count += pxdone;
    ray_count += raysdone;
}
