#pragma once

//Math
#include "../../../../../Common_3/Utilities/Math/MathTypes.h"

#include <vector>

class WorleyNoise3D
{
public:
    WorleyNoise3D(const IVector3& dimension, uint32_t nbSubdiv, int randomSeed);
    ~WorleyNoise3D();

public:
    std::vector<float> generateTexture();
    float evaluate(uint32_t x, uint32_t y, uint32_t z);

private:
    float sample(int row, int col, int depth, const vec3& offset);
    void computeKernel();

private:
    IVector3 m_dimension;
    int32_t m_nbSubDiv;
    int32_t m_sliceDepth;
    int32_t m_rowHeight;
    int32_t m_colWidth;
    int m_randomSeed;

    std::vector<vec3> m_kernelData;
};

