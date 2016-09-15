#ifndef __SAMPLER_HPP__
#define __SAMPLER_HPP__

#include "glm.hpp"
#include <random>
#include <algorithm>
#include <iostream>

class Sampler{
public:
    Sampler(unsigned int seed) : seed(seed) {}
    virtual void Advance() = 0;
    virtual float Get1D() = 0;
    virtual glm::vec2 Get2D() = 0;
    virtual unsigned int GetUsage() const = 0;
protected:
    unsigned int seed;
};

class IndependentSampler : public Sampler{
public:
    IndependentSampler(unsigned int seed) : Sampler(seed), gen(seed){
    }
    virtual void Advance() override {};
    virtual float Get1D() override{
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(gen);
    }
    virtual glm::vec2 Get2D() override{
        float x = std::uniform_real_distribution<float>(0.0f, 1.0f)(gen);
        float y = std::uniform_real_distribution<float>(0.0f, 1.0f)(gen);
        return {x,y};
    }
    virtual unsigned int GetUsage() const override{
        return 0;
    }
private:
    std::mt19937_64 gen;
};

class LatinHypercubeSampler : public Sampler{
public:
    LatinHypercubeSampler(unsigned int seed, unsigned int dim, unsigned int set_size)
        : Sampler(seed),
          samples(dim, std::vector<float>(set_size)),
          dim_count(dim),
          set_size(set_size),
          gen(seed){
        current_sample = 0;
        current_set = -1;
        PrepareSamples();
    }
    virtual void Advance() override{
        current_sample = 0;
        current_set++;
        if(current_set == set_size){
            PrepareSamples();
            current_set = 0;
        }
    }
    virtual float Get1D() override{
        return (current_sample < dim_count) ?
            samples[current_sample++][current_set] :
            std::uniform_real_distribution<float>(0.0f, 1.0f)(gen);
    }
    virtual glm::vec2 Get2D() override{
        float x = (current_sample < dim_count) ?
            samples[current_sample++][current_set] :
            std::uniform_real_distribution<float>(0.0f, 1.0f)(gen);
        float y = (current_sample < dim_count) ?
            samples[current_sample++][current_set] :
            std::uniform_real_distribution<float>(0.0f, 1.0f)(gen);
        return {x,y};
    }
    virtual unsigned int GetUsage() const override{
        return current_sample;
    }
private:
    std::vector<std::vector<float>> samples;
    const unsigned int dim_count;
    const unsigned int set_size;
    unsigned int current_sample;
    unsigned int current_set;
    std::mt19937_64 gen;

    void PrepareSamples(){
        samples = std::vector<std::vector<float>>(dim_count);
        for(unsigned int dim = 0; dim < dim_count; dim++){
            samples[dim] = std::vector<float>(set_size);

            // LHS
            for(unsigned int sample = 0; sample < set_size; sample++){

                float begin = sample/(float)set_size;
                float len = 1.0f/(float)set_size;
                samples[dim][sample] = begin + std::uniform_real_distribution<float>(0.0f, len)(gen);
            }
            std::shuffle(samples[dim].begin(), samples[dim].end(), gen);

            // Independent sampling

            for(unsigned int sample = 0; sample < set_size; sample++){
                samples[dim][sample] = std::uniform_real_distribution<float>(0.0f, 1.0f)(gen);
            }

        }
    }
};

#endif // __SAMPLER_HPP__
