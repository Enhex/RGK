#ifndef __RANDOM_HPP_
#define __RANDOM_HPP_

#include <random>
#include "glm.hpp"
#include <glm/gtx/vector_angle.hpp>

#include <stack>
#include <algorithm>
#include <iostream>

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
    // Returns an independently distributed float in range [from, to].
    float GetFloat(float from, float to){
        return std::uniform_real_distribution<float>(from, to)(gen);
    }
    // Returns a uniformly distributed float in range [from, to].
    // TODO: faster?
    float GetFloat1D(){
        if(LHS_buffer.empty())
            RefillLHSBuffer();

        auto res = LHS_buffer.top();
        LHS_buffer.pop();
        return res.x;
    }
    // Latin Hypercube Sampling
    // TODO: sobol ??
    glm::vec2 GetFloat2D(){
        if(LHS_buffer.empty())
            RefillLHSBuffer();

        auto res = LHS_buffer.top();
        //std::cout << "Taken from buffer" << std::endl;
        LHS_buffer.pop();
        return res;

        // Alternatively, independent sampling
    }
    void RefillLHSBuffer(){
        //std::cout << "Refilling buffer" << std::endl;
        const int size = 64;
        std::vector<unsigned int> V(size);
        for(unsigned int i = 0; i < V.size(); i++) V[i] = i;
        std::vector<unsigned int> W(size);
        for(unsigned int i = 0; i < W.size(); i++) W[i] = i;
        int t = 1000;
        while(t--){
            std::shuffle(V.begin(), V.end(), gen);
            std::shuffle(W.begin(), W.end(), gen);
            for(unsigned int i = 0; i < V.size(); i++){
                unsigned int x = V[i];
                unsigned int y = W[V[i]];
                float xf = GetFloat(x/(float)size, (x+1)/(float)size);
                float yf = GetFloat(y/(float)size, (y+1)/(float)size);
                LHS_buffer.push(glm::vec2(xf,yf));
            }
        }
    }
    // Returns a uniformly distributed point within a disc of given radius.
    glm::vec2 GetDisc(float radius){
        // Rejection sampling.
        glm::vec2 q;
        do{
            q = GetFloat2D()*2.0f - 1.0f;
        }while(q.x*q.x + q.y*q.y > 1.0f);
        return q*radius;
    }
    glm::vec3 MoveToDir(glm::vec3 V, glm::vec3 r){
        glm::vec3 axis = glm::cross(glm::vec3(0.0,1.0,0.0), V);
        // TODO: What if V = -up?
        if(glm::length(axis) < 0.0001f) return (V.y > 0) ? r : -r;
        axis = glm::normalize(axis);
        float angle = glm::angle(glm::vec3(0.0,1.0,0.0), V);
        return glm::rotate(r, angle, axis);
    }
    // Returns a uniformly distributed random vector on a hemishpere, such that y > 0.
    glm::vec3 GetHSUniform(){
        glm::vec2 k = GetFloat2D();
        float k1 = k.x;
        float k2 = k.y*2.0f - 1.0f;
        float z = 1.0f - k2*k2;
        float s = glm::sqrt(z);
        float w = 2.0f*glm::pi<float>()*k1;
        float x = glm::cos(w) * s;
        float y = glm::sin(w) * s;
        return {x,glm::abs(z),y};
    }
    // Returns a uniformly distributed random vector on a hemishpere, such that y > 0.
    glm::vec3 GetHSUniformDir(glm::vec3 V){
        return MoveToDir(V, GetHSUniform());
    }
    // Returns a cosine-distributed random vector on a hemisphere, such that y > 0.
    glm::vec3 GetHSCos(){
        glm::vec2 p = GetDisc(1.0f);
        float y = glm::sqrt(glm::max(0.0f, 1- p.x*p.x - p.y*p.y));
        return glm::vec3(p.x, y, p.y);
    }
    // Returns a cosine-distributed random vector on a hemisphere, such that z > 0.
    glm::vec3 GetHSCosZ(){
        glm::vec2 p = GetDisc(1.0f);
        float z = glm::sqrt(glm::max(0.0f, 1- p.x*p.x - p.y*p.y));
        return glm::vec3(p.x, p.y, z);
    }
    // Returns a cosine-distributed random vector in direction of V.
    glm::vec3 GetHSCosDir(glm::vec3 V){
        return MoveToDir(V, GetHSCos());
    }
    // Returns a uniformly random vector on a sphere
    glm::vec3 GetSphere(float radius){
        glm::vec2 v = GetFloat2D();
        float z = v.x * 2.0f - 1.0f;
        float a = v.y * 6.283185;
        float r = glm::sqrt(1 - z * z);
        float x = r * glm::cos(a);
        float y = r * glm::sin(a);
        return glm::vec3(x, y, z) * radius;
    }
private:
    std::mt19937_64 gen;

    std::stack<glm::vec2> LHS_buffer;
};

#endif // __RANDOM_HPP_
