#include "outbuffer.hpp"

#include <png++/png.hpp>

OutBuffer::OutBuffer(int xsize, int ysize):
    xsize(xsize), ysize(ysize)
{
    data.resize(xsize*ysize);
}

void OutBuffer::SetPixel(int x, int y, Color c)
{
    data[y*xsize + x] = c;
}

void OutBuffer::WriteToPNG(std::string path){

    png::image<png::rgb_pixel> image(xsize, ysize);

    for (png::uint_32 y = 0; y < image.get_height(); ++y){
        for (png::uint_32 x = 0; x < image.get_width(); ++x){
            auto c = data[y*xsize + x];
            auto px = png::rgb_pixel(255.0*c.r,255.0*c.g,255.0*c.b);
            image[y][x] = px;
        }
    }
    image.write(path);
}
