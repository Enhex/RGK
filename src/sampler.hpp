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
    virtual std::pair<unsigned int, unsigned int> GetUsage() const = 0;
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
    virtual std::pair<unsigned int, unsigned int> GetUsage() const override{
        return {0,0};
    }
private:
    std::mt19937_64 gen;
};

class LowDiscrepancyOfflineSampler : public Sampler{
public:
    LowDiscrepancyOfflineSampler(unsigned int seed, unsigned int dim, unsigned int set_size)
        : Sampler(seed),
          samples1D(dim, std::vector<float>(set_size)),
          samples2D(dim, std::vector<glm::vec2>(set_size)),
          dim_count(dim),
          set_size(set_size),
          gen(seed){
        current_sample1D = 0;
        current_sample2D = 0;
        // Advance MUST be called before accessing samples
        current_set = -1;
        // Cannot call PrepareSample from constructor as it is a virtual function!
    }
    virtual void PrepareSamples() = 0;
    virtual void Advance() override{
        if(current_set == (unsigned int)-1) PrepareSamples();
        current_sample1D = 0;
        current_sample2D = 0;
        current_set++;
        qassert_true(current_set < set_size);
    }
    virtual float Get1D() override{
        return (current_sample1D < dim_count) ?
            samples1D[current_sample1D++][current_set] :
            std::uniform_real_distribution<float>(0.0f, 1.0f)(gen);
    }
    virtual glm::vec2 Get2D() override{
        return (current_sample2D < dim_count) ?
            samples2D[current_sample2D++][current_set] :
            glm::vec2(std::uniform_real_distribution<float>(0.0f, 1.0f)(gen),
                      std::uniform_real_distribution<float>(0.0f, 1.0f)(gen));
    }
    virtual std::pair<unsigned int, unsigned int> GetUsage() const override{
        return {current_sample1D,current_sample2D};
    }
protected:
    std::vector<std::vector<float>> samples1D;
    std::vector<std::vector<glm::vec2>> samples2D;
    const unsigned int dim_count;
    const unsigned int set_size;
    unsigned int current_sample1D;
    unsigned int current_sample2D;
    unsigned int current_set;
    std::mt19937_64 gen;
};

class LatinHypercubeSampler : public LowDiscrepancyOfflineSampler{
public:

    LatinHypercubeSampler(unsigned int seed, unsigned int dim, unsigned int set_size)
        : LowDiscrepancyOfflineSampler(seed, dim, set_size){

    }
    virtual void PrepareSamples() override{
        for(unsigned int dim = 0; dim < dim_count; dim++){
            samples1D[dim] = std::vector<float>(set_size);
            samples2D[dim] = std::vector<glm::vec2>(set_size);

            // 1D LHS
            for(unsigned int sample = 0; sample < set_size; sample++){
                float begin = sample/(float)set_size;
                float len = 1.0f/(float)set_size;
                samples1D[dim][sample] = begin + std::uniform_real_distribution<float>(0.0f, len)(gen);
            }
            std::shuffle(samples1D[dim].begin(), samples1D[dim].end(), gen);

            // 2D LHS
            std::vector<unsigned int> LHS(set_size);
            for(unsigned int i = 0; i < set_size; i++) LHS[i] = i;
            std::shuffle(LHS.begin(), LHS.end(), gen);
            for(unsigned int sample = 0; sample < set_size; sample++){
                float xbegin = sample/(float)set_size;
                float ybegin = LHS[sample]/(float)set_size;
                float len = 1.0f/(float)set_size;
                float x = xbegin + std::uniform_real_distribution<float>(0.0f, len)(gen);
                float y = ybegin + std::uniform_real_distribution<float>(0.0f, len)(gen);
                samples2D[dim][sample] = glm::vec2(x,y);
            }
            std::shuffle(samples2D[dim].begin(), samples2D[dim].end(), gen);

            // Alternatively, for comparison, independent sampling
            /*
              for(unsigned int sample = 0; sample < set_size; sample++){
              samples1D[dim][sample] = std::uniform_real_distribution<float>(0.0f, 1.0f)(gen);
              samples2D[dim][sample] = glm::vec2(std::uniform_real_distribution<float>(0.0f, 1.0f)(gen),
              std::uniform_real_distribution<float>(0.0f, 1.0f)(gen));
              }
            */

        }
    }
};

#endif // __SAMPLER_HPP__
