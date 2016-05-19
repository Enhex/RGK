#ifndef __RANDOM_HPP_
#define __RANDOM_HPP_

#include <random>
#include <glm/gtx/vector_angle.hpp>

#include "glm.hpp"

class Random{
public:
    Random(uint64_t seed) :
        gen(seed)
    {
    }

    // Pinches the seed by specified number.
    void Shift(uint64_t x){
        gen.seed(x + gen());
    }

    // Returns a uniformly distributed number in range [0,1].
    float Get01(){
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(gen);
    }
    // Returns a uniformly distributed int in range [from, to].
    int GetInt(float from, float to){
        return std::uniform_int_distribution<>(from, to)(gen);
    }
    // Returns a uniformly distributed float in range [from, to].
    float GetFloat(float from, float to){
        return std::uniform_real_distribution<float>(from, to)(gen);
    }
    // Returns a uniformly distributed point within a disc of given radius.
    glm::vec2 GetDisc(float radius){
        // Rejection sampling.
        glm::vec2 q;
        do{
            q.x = GetFloat(-1.0, 1.0);
            q.y = GetFloat(-1.0, 1.0);
        }while(q.x*q.x + q.y*q.y > 1.0f);
        return q*radius;
    }
    // Returns a cosine-distributed random vector on a hemisphere, such that y > 0.
    glm::vec3 GetHSCos(){
        glm::vec2 p = GetDisc(1.0f);
        float y = glm::sqrt(glm::max(0.0f, 1- p.x*p.x - p.y*p.y));
        return glm::vec3(p.x, y, p.y);
    }
    // Returns a cosine-distributed random vector in direction of V.
    glm::vec3 GetHSCosDir(glm::vec3 V){
        glm::vec3 r = GetHSCos();
        glm::vec3 axis = glm::cross(glm::vec3(0.0,1.0,0.0), V);
        // TODO: What if V = -up?
        if(glm::length(axis) < 0.0001f) return (V.y > 0) ? r : -r;
        axis = glm::normalize(axis);
        float angle = glm::angle(glm::vec3(0.0,1.0,0.0), V);
        return glm::rotate(r, angle, axis);
    }
    // Returns a uniformly random vector on a sphere
    glm::vec3 GetSphere(float radius){
        float z = GetFloat(-1, 1);
        float a = GetFloat(0, 6.283185);
        float r = glm::sqrt(1 - z * z);
        float x = r * glm::cos(a);
        float y = r * glm::sin(a);
        return glm::vec3(x, y, z) * radius;
    }
private:
    std::mt19937_64 gen;
};

#endif // __RANDOM_HPP_
