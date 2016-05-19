#ifndef __TEXTURE_HPP__
#define __TEXTURE_HPP__

#include <string>
#include <vector>
#include <mutex>

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

    // Finite differentes for bump maps
    float GetSlopeRight(glm::vec2 pos);
    float GetSlopeBottom(glm::vec2 pos);

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

class EXRTexture{
public:
    EXRTexture(int xsize, int ysize);
    EXRTexture(const EXRTexture& other) :
        xsize(other.xsize), ysize(other.ysize),
        data(other.data),  count(other.count) {}
    bool Write(std::string path) const;
    void AddPixel(int x, int y, Radiance c);
    Radiance GetPixel(int x, int y) const;
    EXRTexture Normalize() const;

private:
    unsigned int xsize, ysize;
    std::vector<Radiance> data;
    std::vector<unsigned int> count;

    mutable std::mutex mx;
};

#endif // __TEXTURE_HPP__
