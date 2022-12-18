#pragma once

//Math
#include "../../../../../../Common_3/Utilities/Math/MathTypes.h"

#include <vector>

class PerlinNoise3D
{
public:
    PerlinNoise3D(const IVector3& textureDim, uint32_t kernelSize, std::size_t nbLayers, float scaleFactor, int randomSeed);
    ~PerlinNoise3D();

public:
    std::vector<float> generateTexture();
    float evaluate(uint32_t x, uint32_t y, uint32_t z);

private:
    float sample(float x, float y, float z);
    float computeNoiseValue(float x, float y, float z) const;
    void computeKernelDirection();
    int hash(int x, int y, int z) const;
    float smoothstep(float val) const;
    float interpolate(float min, float max, float value) const;

private:
    IVector3 m_textureDim;
    uint32_t m_kernelSize;
    size_t m_nbLayers;
    float m_scaleFactor;
    int m_randomSeed;
    float m_baseFrequency;
    float m_rateOffChanged;

    std::vector<vec3> m_kernelDirections;
    std::vector<int> m_permutationTable;
};

