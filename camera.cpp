#include "camera.hpp"

#include <glm/gtc/random.hpp>

Camera::Camera(glm::vec3 pos, glm::vec3 la, glm::vec3 up, float yview, float xview, float focus_plane, float ls){
    origin = pos;
    lookat = la;
    cameraup = up;

    lens_size = ls;

    direction = glm::normalize( lookat - origin);
    cameraleft = glm::normalize(glm::cross(direction, cameraup));
    cameraup = glm::normalize(glm::cross(direction,cameraleft));

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


Ray Camera::GetRandomRay(int x, int y, int xres, int yres) const{
    glm::vec2 off(glm::linearRand(0.0f,1.0f), glm::linearRand(0.0f, 1.0f));
    glm::vec3 p = GetViewScreenPoint( (x + off.x) / (float)(xres),
                                      (y + off.y) / (float)(yres) );
    glm::vec3 o = origin;
    return Ray(o, p - o);
}

Ray Camera::GetRandomRayLens(int x, int y, int xres, int yres) const{
    glm::vec2 off(glm::linearRand(0.0f,1.0f), glm::linearRand(0.0f, 1.0f));
    glm::vec3 p = GetViewScreenPoint( (x + off.x) / (float)(xres),
                                      (y + off.y) / (float)(yres) );
    glm::vec2 lenso = glm::diskRand(lens_size);
    glm::vec3 o = origin + lenso.x * cameraleft + lenso.y * cameraup;
    return Ray(o, p - o);
}
