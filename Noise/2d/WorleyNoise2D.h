#pragma once

//Math
#include "../../../../../Common_3/Utilities/Math/MathTypes.h"

#include <vector>

class WorleyNoise2D
{
public:
    WorleyNoise2D(const IVector2& dimension, uint32_t nbSubdiv);
    ~WorleyNoise2D();

public:
    std::vector<float> generateTexture();
    float evaluate(uint32_t x, uint32_t y);

private:
    float sample(int row, int col, const vec2& offset);
    void computeKernel();

private:
    IVector2 m_dimension;
    int32_t m_nbSubDiv;
    int32_t m_rowHeight;
    int32_t m_colWidth;
    int m_randomSeed;

    std::vector<vec2> m_kernelData;
};

