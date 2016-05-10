#include "tracer.hpp"
#include "texture.hpp"

extern bool debug_trace;
extern unsigned int debug_x, debug_y;

void Tracer::Render(const RenderTask& task, EXRTexture* output, std::atomic<int>& pixel_count, std::atomic<unsigned int>& ray_count){
    unsigned int pxdone = 0, raysdone = 0;
    for(unsigned int y = task.yrange_start; y < task.yrange_end; y++){
        for(unsigned int x = task.xrange_start; x < task.xrange_end; x++){
            bool d = false;
            if(debug_trace && x == debug_x && y == debug_y) d = true;

            Radiance px = RenderPixel(x, y, raysdone, d);

            output->SetPixel(x, y, px);
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
