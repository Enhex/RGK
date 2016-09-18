#ifndef __SAMPLER_HPP__
#define __SAMPLER_HPP__

#include "glm.hpp"
#include "out.hpp"
#include <random>
#include <algorithm>

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

class OfflineSampler : public Sampler{
public:
    OfflineSampler(unsigned int seed, unsigned int dim, unsigned int set_size)
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

class LatinHypercubeSampler : public OfflineSampler{
public:
    LatinHypercubeSampler(unsigned int seed, unsigned int dim, unsigned int set_size)
        : OfflineSampler(seed, dim, set_size){
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
        }
    }
};

class IndependentOfflineSampler : public OfflineSampler{
public:
    IndependentOfflineSampler(unsigned int seed, unsigned int dim, unsigned int set_size)
        : OfflineSampler(seed, dim, set_size){
    }

    virtual void PrepareSamples() override{
        for(unsigned int dim = 0; dim < dim_count; dim++){
            for(unsigned int sample = 0; sample < set_size; sample++){
                samples1D[dim][sample] = std::uniform_real_distribution<float>(0.0f, 1.0f)(gen);
                samples2D[dim][sample] = glm::vec2(std::uniform_real_distribution<float>(0.0f, 1.0f)(gen),
                                                   std::uniform_real_distribution<float>(0.0f, 1.0f)(gen));
            }
        }
    }
};

static unsigned int round_up_to_square(unsigned int x){
    float s = std::sqrt(x);
    float i;
    float frac = std::modf(s, &i);
    if(frac < 0.0001f) return i*i;
    else return (i+1)*(i+1);
}

class StratifiedSampler : public OfflineSampler{
public:
    StratifiedSampler(unsigned int seed, unsigned int dim, unsigned int ssize)
        : OfflineSampler(seed, dim, round_up_to_square(ssize)){
        if(ssize != set_size){
            out::cout(6) << "Stratified sampler rounded set_size up from " << ssize << " to " << set_size << std::endl;
        }
    }
    virtual void PrepareSamples() override {
        for(unsigned int dim = 0; dim < dim_count; dim++){
            for(unsigned int sample = 0; sample < set_size; sample++){
                float begin = sample/(float)set_size;
                float len = 1.0f/(float)set_size;
                samples1D[dim][sample] = begin + std::uniform_real_distribution<float>(0.0f, len)(gen);
            }
            std::shuffle(samples1D[dim].begin(), samples1D[dim].end(), gen);

            unsigned int sq = std::sqrt(set_size) + 0.5f;

            for(unsigned int sy = 0; sy < sq; sy++){
                for(unsigned int sx = 0; sx < sq; sx++){

                    float len = 1.0f/(float)sq;
                    float beginx = sx/(float)sq;
                    float beginy = sy/(float)sq;
                    float x = beginx + std::uniform_real_distribution<float>(0.0f, len)(gen);
                    float y = beginy + std::uniform_real_distribution<float>(0.0f, len)(gen);

                    samples2D[dim][sy*sq + sx] = glm::vec2(x,y);
                }
            }
            std::shuffle(samples2D[dim].begin(), samples2D[dim].end(), gen);
        }
    }

};

#endif // __SAMPLER_HPP__
