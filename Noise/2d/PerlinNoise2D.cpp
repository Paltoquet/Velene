#include "PerlinNoise2D.h"

#include <cmath> 
#include <cstdio> 
#include <random> 
#include <functional> 

#define PI 3.14159265358979323846f

PerlinNoise2D::PerlinNoise2D(const IVector2& textureDim, const IVector2& kernelSize, size_t nbLayers, float scaleFactor, int randomSeed) :
    m_textureDim(textureDim),
    m_kernelSize(kernelSize),
    m_nbLayers(nbLayers),
    m_randomSeed(randomSeed),
    m_scaleFactor(scaleFactor),
    m_baseFrequency(0.05f),
    m_rateOffChanged(2.0f)
{
    computeKernelDirection();
}

PerlinNoise2D::~PerlinNoise2D()
{

}

/* --------------------------------- Public methods --------------------------------- */

std::vector<float> PerlinNoise2D::generateTexture()
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

float PerlinNoise2D::evaluate(uint32_t x, uint32_t y)
{
    return sample(float(x), float(y));
}

float PerlinNoise2D::sample(float x, float y)
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

float PerlinNoise2D::computeNoiseValue(float x, float y) const
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

    // gradients at the corner of the cell
    const vec2& d00 = m_kernelDirections[hash(rx0, ry0)];
    const vec2& d10 = m_kernelDirections[hash(rx1, ry0)];
    const vec2& d01 = m_kernelDirections[hash(rx0, ry1)];
    const vec2& d11 = m_kernelDirections[hash(rx1, ry1)];

    // generate vectors going from the grid points to p
    float x0 = tx, x1 = tx - 1;
    float y0 = ty, y1 = ty - 1;

    vec2 p00 = vec2(x0, y0);
    vec2 p10 = vec2(x1, y0);
    vec2 p01 = vec2(x0, y1);
    vec2 p11 = vec2(x1, y1);

    // remapping of tx and ty using the Smoothstep function 
    float sx = smoothstep(tx);
    float sy = smoothstep(ty);

    float a = interpolate(Vectormath::dot(d00, p00), Vectormath::dot(d10, p10), sx);
    float b = interpolate(Vectormath::dot(d01, p01), Vectormath::dot(d11, p11), sx);

    // linearly interpolate the nx0/nx1 along they y axis
    return (interpolate(a, b, sy) + 1.0f) / 2.0f;
}

void PerlinNoise2D::computeKernelDirection()
{
    size_t kernelSize = m_kernelSize.getX();
    m_kernelDirections.clear();
    m_kernelDirections.resize(kernelSize);
    m_permutationTable.resize(2 * kernelSize);

    std::mt19937 generator(m_randomSeed);
    std::uniform_real_distribution<float> distribution;
    auto dice = std::bind(distribution, generator);

    for (unsigned i = 0; i < kernelSize; ++i) {
        // better
        //float theta = acos(2 * dice() - 1);
        //float phi = 2 * dice() * PI;
        //
        //float x = cos(phi) * sin(theta);
        //float y = sin(phi) * sin(theta);
        //m_kernelDirections[i] = vec2(x, y);

        m_kernelDirections[i] = Vectormath::normalize(vec2(2.0f * dice() - 1.0f, 2.0f * dice() - 1.0f));
        m_permutationTable[i] = i;
    }

    std::uniform_int_distribution<unsigned> distributionInt;
    auto diceInt = std::bind(distributionInt, generator);

    for (unsigned i = 0; i < kernelSize; ++i) {
        std::swap(m_permutationTable[i], m_permutationTable[diceInt() % kernelSize]);
        m_permutationTable[kernelSize + i] = m_permutationTable[i];
    }
}

float PerlinNoise2D::smoothstep(float val) const
{
    return val * val * (3.0f - 2.0f * val);
}

float PerlinNoise2D::interpolate(float min, float max, float value) const
{
    return min * (1.0f - value) + max * value;
}

int PerlinNoise2D::hash(int x, int y) const
{
   return m_permutationTable[m_permutationTable[x] + y];
}