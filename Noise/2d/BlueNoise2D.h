#pragma once

#include <vector>
#include <functional>

//Math
#include "../../../../../../Common_3/Utilities/Math/MathTypes.h"

class BlueNoise2D
{
public:
    BlueNoise2D(const IVector2& textureDim);
    ~BlueNoise2D();

public:
    std::vector<float> generateTexture();
    float evaluate(uint32_t x, uint32_t y);

private:
    IVector2 m_textureDim;
    int m_randomSeed;
    std::function<float()> m_dice;
};

