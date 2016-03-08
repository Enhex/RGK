#ifndef __OUTBUFFER_HPP__
#define __OUTBUFFER_HPP__

#include <string>
#include <vector>

#include "ray.hpp"

class OutBuffer{
public:
    OutBuffer(int xsize, int ysize);

    void WriteToPNG(std::string path);
    void SetPixel(int x, int y, Color c);
private:
    // Fixed size is kept manually.
    std::vector<Color> data;
    int xsize, ysize;
};

#endif // __OUTBUFFER_HPP__
