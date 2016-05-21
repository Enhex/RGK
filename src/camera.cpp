#include "camera.hpp"

#include "glm.hpp"
#include <glm/gtx/polar_coordinates.hpp>

Camera::Camera(glm::vec3 pos, glm::vec3 la, glm::vec3 up, float yview, float xview, float focus_plane, float ls){
    origin = pos;
    lookat = la;
    cameraup = up;

    lens_size = ls;

    direction = glm::normalize( lookat - origin);
    cameraleft = glm::normalize(glm::cross(cameraup, direction));
    cameraup = glm::normalize(glm::cross(cameraleft, direction));

    viewscreen_x = - xview * cameraleft * focus_plane;
    viewscreen_y =   yview * cameraup * focus_plane;
    viewscreen = origin + direction * focus_plane - 0.5f * viewscreen_y - 0.5f * viewscreen_x;
}

glm::vec3 Camera::GetViewScreenPoint(float x, float y) const {
    glm::vec3 xo = x * viewscreen_x;
    glm::vec3 yo = y * viewscreen_y;
    return viewscreen + xo + yo;
}

Ray Camera::GetSubpixelRay(int x, int y, int xres, int yres, int subx, int suby, int subres) const {
    glm::vec2 off( subx/(float)subres, suby/(float)subres );
    glm::vec3 p = GetViewScreenPoint( (x + off.x) / (float)(xres),
                                      (y + off.y) / (float)(yres) );
    glm::vec3 o = origin;
    return Ray(o, p - o);
}


Ray Camera::GetRandomRay(int x, int y, int xres, int yres, Random& rnd) const{
    glm::vec2 off(rnd.Get01(), rnd.Get01());
    glm::vec3 p = GetViewScreenPoint( (x + off.x) / (float)(xres),
                                      (y + off.y) / (float)(yres) );
    glm::vec3 o = origin;
    return Ray(o, p - o);
}

Ray Camera::GetCenterRay(int x, int y, int xres, int yres) const{
    glm::vec2 off(0.5f, 0.5f);
    glm::vec3 p = GetViewScreenPoint( (x + off.x) / (float)(xres),
                                      (y + off.y) / (float)(yres) );
    glm::vec3 o = origin;
    return Ray(o, p - o);
}

Ray Camera::GetRandomRayLens(int x, int y, int xres, int yres, Random& rnd) const{
    glm::vec2 off(rnd.Get01(), rnd.Get01());
    glm::vec3 p = GetViewScreenPoint( (x + off.x) / (float)(xres),
                                      (y + off.y) / (float)(yres) );
    glm::vec2 lenso = rnd.GetDisc(lens_size);
    glm::vec3 o = origin + lenso.x * cameraleft + lenso.y * cameraup;
    return Ray(o, p - o);
}

Ray Camera::GetSubpixelRayLens(int x, int y, int xres, int yres, int subx, int suby, int subres, Random& rnd) const {
    glm::vec2 off( subx/(float)subres, suby/(float)subres );
    glm::vec3 p = GetViewScreenPoint( (x /*+ off.x*/) / (float)(xres),
                                      (y /*+ off.y*/) / (float)(yres) );
    //glm::vec2 lenso = (off - 0.5f)*2.0f * lens_size;
    //glm::vec2 lenso = glm::euclidean(glm::vec2(off.y * 2*3.14f + off.x*15.99f, off.x*0.8f + 0.2f)).yz() * lens_size;
    glm::vec2 lenso = rnd.GetDisc(lens_size);
    glm::vec3 o = origin + lenso.x * cameraleft + lenso.y * cameraup;
    return Ray(o, p - o);
}
