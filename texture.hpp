#ifndef __TEXTURE_HPP__
#define __TEXTURE_HPP__

#include <string>
#include <vector>

#include "primitives.hpp"
#include "ray.hpp"

class Texture{
public:
    Texture(int xsize, int ysize);

    void WriteToPNG(std::string path);
    void WriteToBMP(std::string path);
    void SetPixel(int x, int y, Color c);
    void GetPixel(int x, int y);

    Color GetPixelInterpolated(glm::vec2 pos, bool debug = false);

    static Texture* CreateNewFromPNG(std::string path);
private:
    // Fixed size is kept manually.
    std::vector<Color> data;
    int xsize, ysize;

    Texture();
};

#endif // __TEXTURE_HPP__
