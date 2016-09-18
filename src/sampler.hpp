#ifndef __SAMPLER_HPP__
#define __SAMPLER_HPP__

#include "glm.hpp"
#include "out.hpp"
#include <random>
#include <algorithm>

#include "../external/halton_sampler.h"

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
    std::mt19937 gen;
};

class OfflineSampler : public Sampler{
public:
    OfflineSampler(unsigned int seed, unsigned int dim, unsigned int set_size);
    virtual void PrepareSamples() = 0;
    virtual void Advance() override;
    virtual float Get1D() override;
    virtual glm::vec2 Get2D() override;
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
    std::mt19937 gen;
};

class LatinHypercubeSampler : public OfflineSampler{
public:
    LatinHypercubeSampler(unsigned int seed, unsigned int dim, unsigned int set_size)
        : OfflineSampler(seed, dim, set_size){
    }
    virtual void PrepareSamples() override;
};

class IndependentOfflineSampler : public OfflineSampler{
public:
    IndependentOfflineSampler(unsigned int seed, unsigned int dim, unsigned int set_size)
        : OfflineSampler(seed, dim, set_size){
    }
    virtual void PrepareSamples() override;
};

class StratifiedSampler : public OfflineSampler{
public:
    StratifiedSampler(unsigned int seed, unsigned int dim, unsigned int ssize);
    virtual void PrepareSamples() override;
};

class VanDerCoruptSampler : public OfflineSampler{
public:
    VanDerCoruptSampler(unsigned int seed, unsigned int dim, unsigned int set_size)
        : OfflineSampler(seed, dim, set_size){
    }
    virtual void PrepareSamples() override;
};


// Halton sequence

template<unsigned int base>
class Halton {
public:
    Halton(){
		inv_base = 1.0f/base;
        value = 0.0f;
    }
	Halton(int i) {
		float f = inv_base = 1.0f/base;
		value = 0.0f;
		while(i>0) {
			value += f*(double)(i%base);
			i /= base;
			f *= inv_base;
		}
	}
	float next() {
        float v = value;
		float r = 1.0 - value - 0.0000001;
		if(inv_base<r) value += inv_base;
		else {
			double h = inv_base, hh;
			do {hh = h; h *= inv_base;} while(h >=r);
			value += hh + h - 1.0;
		}
        return v;
	}
private:
	float value;
    float inv_base;
};


template<int base1d, int base2dA, int base2dB>
class HaltonSampler : public OfflineSampler{
public:
    HaltonSampler(unsigned int seed, unsigned int dim, unsigned int set_size)
        : OfflineSampler(seed, dim, set_size){
        hs.init_faure();
    }
    virtual void PrepareSamples() override{
        for(unsigned int dim = 0; dim < dim_count; dim++){
            for(unsigned int sample = 0; sample < set_size; sample++){
                samples1D[dim][sample] = hs.sample(base1d,sample+1);
            }
            std::shuffle(samples1D[dim].begin(), samples1D[dim].end(), gen);

            for(unsigned int sample = 0; sample < set_size; sample++){
                samples2D[dim][sample] = {hs.sample(base2dA,sample+1), hs.sample(base2dB,sample+1)};
            }
            std::shuffle(samples2D[dim].begin(), samples2D[dim].end(), gen);
        }
    }
private:
    HS::Halton_sampler hs;
};

#endif // __SAMPLER_HPP__
