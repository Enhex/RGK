#ifndef __TEXTURE_HPP__
#define __TEXTURE_HPP__

#include <string>
#include <vector>
#include <mutex>

#include "radiance.hpp"

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
    static Texture* CreateNewFromHDR(std::string path);

    void FillStripes(unsigned int size, Color a, Color b);
private:
    // Fixed size is kept manually.
    std::vector<Color> data;
    unsigned int xsize, ysize;

    Texture();
};

class EXRTexture{
public:
    EXRTexture(int xsize = 0, int ysize = 0);
    EXRTexture(const EXRTexture& other) :
        xsize(other.xsize), ysize(other.ysize),
        data(other.data),  count(other.count) {
    }
    EXRTexture(EXRTexture&& other){
        std::swap(xsize,other.xsize);
        std::swap(ysize,other.ysize);
        std::swap(data,other.data);
        std::swap(count,other.count);
    }
    EXRTexture& operator=(EXRTexture&& other){
        std::swap(xsize,other.xsize);
        std::swap(ysize,other.ysize);
        std::swap(data,other.data);
        std::swap(count,other.count);
        return *this;
    }
    bool Write(std::string path) const;
    void AddPixel(int x, int y, Radiance c, unsigned int n = 1);
    Radiance GetPixel(int x, int y) const;
    // A positive value will be applied as a scaling factor to the entire texture.
    // A negative value enables automatic scaling factor detection
    EXRTexture Normalize(float val) const;

    void Accumulate(const EXRTexture& other);

private:
    unsigned int xsize, ysize;
    std::vector<Radiance> data;
    std::vector<unsigned int> count;

    mutable std::mutex mx;
};

#endif // __TEXTURE_HPP__
