#pragma once

//Math
#include "../../../../../Common_3/Utilities/Math/MathTypes.h"

#include <vector>

class PerlinNoise2D
{
public:
    PerlinNoise2D(const IVector2& dimension, float noiseScale);
    ~PerlinNoise2D();

public:
    float evaluate(uint32_t xPixel, uint32_t yPixel);

private:
    float grad2(int hash, float x, float y);

private:
    IVector2 m_dimension;
    float m_noiseScale;
};

