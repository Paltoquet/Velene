#pragma once

//Math
#include "../../../../../../Common_3/Utilities/Math/MathTypes.h"

#include <vector>

class ValueNoise2D
{
public:
    ValueNoise2D(const IVector2& textureDim, const IVector2& kernelSize, std::size_t nbLayers, float scaleFactor);
    ~ValueNoise2D();

public:
    std::vector<float> generateTexture();
    float evaluate(uint32_t x, uint32_t y);

private:
    float sample(float x, float y);
    float computeNoiseValue(float x, float y) const;
    void computeKernel();
    float smoothstep(float val) const;
    float interpolate(float min, float max, float value) const;

private:
    IVector2 m_textureDim;
    IVector2 m_kernelSize;
    size_t m_nbLayers;
    float m_scaleFactor;
    int m_randomSeed;
    float m_baseFrequency;
    float m_rateOffChanged;

    std::vector<float> m_kernelData;
};

