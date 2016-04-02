#ifndef __TEXTURE_HPP__
#define __TEXTURE_HPP__

#include <string>
#include <vector>

#include "primitives.hpp"
#include "ray.hpp"

class Texture{
public:
    Texture(int xsize, int ysize);

    bool Write(std::string path) const;
    void WriteToPNG(std::string path) const;
    void WriteToBMP(std::string path) const;
    void SetPixel(int x, int y, Color c);
    void GetPixel(int x, int y);

    Color GetPixelInterpolated(glm::vec2 pos, bool debug = false) const;

    static Texture* CreateNewFromPNG(std::string path);
    static Texture* CreateNewFromJPEG(std::string path);

    void FillStripes(unsigned int size, Color a, Color b);
private:
    // Fixed size is kept manually.
    std::vector<Color> data;
    unsigned int xsize, ysize;

    Texture();
};

#endif // __TEXTURE_HPP__
