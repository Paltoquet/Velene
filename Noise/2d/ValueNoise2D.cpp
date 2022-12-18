#include "ValueNoise2D.h"

#include <cmath> 
#include <cstdio> 
#include <random> 
#include <functional> 

ValueNoise2D::ValueNoise2D(const IVector2& textureDim, const IVector2& kernelSize, size_t nbLayers, float scaleFactor) :
    m_textureDim(textureDim),
    m_kernelSize(kernelSize),
    m_nbLayers(nbLayers),
    m_randomSeed(42),
    m_scaleFactor(scaleFactor),
    m_baseFrequency(0.05f),
    m_rateOffChanged(2.0f)
{
    computeKernel();
}

ValueNoise2D::~ValueNoise2D()
{

}

/* --------------------------------- Public methods --------------------------------- */

std::vector<float> ValueNoise2D::generateTexture()
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

float ValueNoise2D::evaluate(uint32_t x, uint32_t y)
{
    return sample(float(x), float(y));
}

float ValueNoise2D::sample(float x, float y)
{
    float brownianNoise = 0.0;
    float noiseMax = 0.0;

    for (size_t i = 0; i < m_nbLayers; ++i)
    {
        float amplitude = float(pow(m_rateOffChanged, i));
        float frequency = m_baseFrequency * amplitude * m_scaleFactor;
        brownianNoise += computeNoiseValue(x * frequency, y * frequency) / amplitude;
        noiseMax += 1.0f / amplitude;
    }
    brownianNoise = brownianNoise / noiseMax;
    return brownianNoise;
}

/* --------------------------------- Private methods --------------------------------- */

float ValueNoise2D::computeNoiseValue(float x, float y) const
{
    int xi = int(std::floor(x));
    int yi = int(std::floor(y));

    float tx = x - xi;
    float ty = y - yi;

    int kernelWidth = m_kernelSize.getX();
    int kernelHeight = m_kernelSize.getY();

    int rx0 = xi % kernelWidth;
    int rx1 = (rx0 + 1) % kernelWidth;
    int ry0 = yi % kernelHeight;
    int ry1 = (ry0 + 1) % kernelHeight;

    // random values at the corners of the cell using permutation table
    const float& c00 = m_kernelData[ry0 * kernelWidth + rx0];
    const float& c10 = m_kernelData[ry0 * kernelWidth + rx1];
    const float& c01 = m_kernelData[ry1 * kernelWidth + rx0];
    const float& c11 = m_kernelData[ry1 * kernelWidth + rx1];

    // remapping of tx and ty using the Smoothstep function 
    float sx = smoothstep(tx);
    float sy = smoothstep(ty);

    // linearly interpolate values along the x axis
    float nx0 = interpolate(c00, c10, sx);
    float nx1 = interpolate(c01, c11, sx);

    // linearly interpolate the nx0/nx1 along they y axis
    return interpolate(nx0, nx1, sy);
}

void ValueNoise2D::computeKernel()
{
    size_t kernel_size = m_kernelSize.getX() * m_kernelSize.getY();
    m_kernelData.clear();
    m_kernelData.resize(kernel_size);

    std::mt19937 gen(m_randomSeed);
    std::uniform_real_distribution<float> distrFloat;
    auto randFloat = std::bind(distrFloat, gen);

    for (unsigned k = 0; k < kernel_size; ++k) {
        m_kernelData[k] = randFloat();
    }
}

float ValueNoise2D::smoothstep(float val) const
{
    return val * val * (3.0f - 2.0f * val);
}

float ValueNoise2D::interpolate(float min, float max, float value) const
{
    return min * (1.0f - value) + max * value;
}