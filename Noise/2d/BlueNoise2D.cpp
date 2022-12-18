#include "BlueNoise2D.h"

#include <cmath> 
#include <cstdio> 
#include <random> 
#include <functional> 

#define PI 3.14159265358979323846f

BlueNoise2D::BlueNoise2D(const IVector2& textureDim) :
    m_textureDim(textureDim),
    m_randomSeed(42)
{
    std::mt19937 generator(m_randomSeed);
    std::uniform_real_distribution<float> distribution;
    m_dice = std::function<float()>(std::bind(distribution, generator));
}

BlueNoise2D::~BlueNoise2D()
{

}

/* --------------------------------- Public methods --------------------------------- */

std::vector<float> BlueNoise2D::generateTexture()
{
    std::vector<float> result;
    int width = m_textureDim.getX();
    int height = m_textureDim.getY();

    result.resize(width * height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float val = evaluate(x, y);
            result[y * width + x] = val;
        }
    }

    return result;
}

float BlueNoise2D::evaluate(uint32_t x, uint32_t y)
{
    return m_dice();
}
