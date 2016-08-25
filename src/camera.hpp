#ifndef __CAMERA_HPP__
#define __CAMERA_HPP__

#include "glm.hpp"
#include "ray.hpp"
#include "random.hpp"

class Camera{
public:
    Camera(glm::vec3 pos, glm::vec3 la, glm::vec3 up, float yview, float xview, int xsize, int ysize, float focus_plane = 1.0f, float lens_size = 0.0f);

    /* xres, yres - targetted output image resolution
       x, y - pixel coordinates
       subres - grid resolution for dividning the (x,y) pixel into subpixels
       subx, suby - the coordinates of subpixel within (x,y) pixel for requesting ray
     */
    Ray GetSubpixelRay(int x, int y, int xres, int yres, int subx, int suby, int subres) const;
    Ray GetSubpixelRayLens(int x, int y, int xres, int yres, int subx, int suby, int subres, Random& rnd) const;

    Ray GetSubpixelRayRandom(int x, int y, int xres, int yres, int subx, int suby, int subres, Random& rnd) const;
    Ray GetSubpixelRayLensRandom(int x, int y, int xres, int yres, int subx, int suby, int subres, Random& rnd) const;

    Ray GetRandomRay(int x, int y, int xres, int yres, Random& rnd) const;
    Ray GetRandomRayLens(int x, int y, int xres, int yres, Random& rnd) const;
    Ray GetCenterRay(int x, int y, int xres, int yres) const;

    bool IsSimple() const {return lens_size == 0.0f;}

    // Returns false if direction is not within camera view
    bool GetCoordsFromDirection(glm::vec3 dir, int& /*out*/ x, int& /*out*/ y, bool debug = false) const;
private:
    glm::vec3 origin;
    glm::vec3 lookat;
    glm::vec3 direction;

    glm::vec3 cameraup;
    glm::vec3 cameraleft;

    glm::vec3 viewscreen;
    glm::vec3 viewscreen_x;
    glm::vec3 viewscreen_y;

    float lens_size;

    int xsize;
    int ysize;

    glm::vec3 GetViewScreenPoint(float x, float y) const;

};

#endif // __CAMERA_HPP__
