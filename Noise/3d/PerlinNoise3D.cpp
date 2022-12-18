#include "PerlinNoise3D.h"

#include "../../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"

#include <cmath> 
#include <cstdio> 
#include <random> 
#include <functional> 

#define PI 3.14159265358979323846f

PerlinNoise3D::PerlinNoise3D(const IVector3& textureDim, uint32_t kernelSize, size_t nbLayers, float scaleFactor, int randomSeed) :
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

PerlinNoise3D::~PerlinNoise3D()
{

}

/* --------------------------------- Public methods --------------------------------- */

std::vector<float> PerlinNoise3D::generateTexture()
{
    std::vector<float> result;
    int width = m_textureDim.getX();
    int height = m_textureDim.getY();
    int depth = m_textureDim.getZ();
    int pageSize = width * height;

    result.resize(width * height * depth);
    for (int z = 0; z < height; z++) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float val = evaluate(x, y, z);
                result[z * pageSize + y * width + x] = val;
            }
        }
    }

    return result;
}

float PerlinNoise3D::evaluate(uint32_t x, uint32_t y, uint32_t z)
{
    return sample(float(x), float(y), float(z));
}

float PerlinNoise3D::sample(float x, float y, float z)
{
    float brownianNoise = 0.0;
    float noiseMax = 0.0;

    for (size_t i = 0; i < m_nbLayers; ++i)
    {
        float amplitude = float(pow(m_rateOffChanged, i));
        float frequency = m_baseFrequency * amplitude * m_scaleFactor;
        brownianNoise += computeNoiseValue(x * frequency, y * frequency, z * frequency) / amplitude;
        noiseMax += 1.0f / amplitude;
    }
    brownianNoise = brownianNoise / noiseMax;
    return brownianNoise;
}

/* --------------------------------- Private methods --------------------------------- */

float PerlinNoise3D::computeNoiseValue(float x, float y, float z) const
{
    int xi = int(std::floor(x));
    int yi = int(std::floor(y));
    int zi = int(std::floor(z));

    float tx = x - xi;
    float ty = y - yi;
    float tz = z - zi;

    int rx0 = xi % m_kernelSize;
    int rx1 = (rx0 + 1) % m_kernelSize;
    int ry0 = yi % m_kernelSize;
    int ry1 = (ry0 + 1) % m_kernelSize;
    int rz0 = zi % m_kernelSize;
    int rz1 = (rz0 + 1) % m_kernelSize;

    // gradients at the corner of the cell
    const vec3& d000 = m_kernelDirections[hash(rx0, ry0, rz0)];
    const vec3& d100 = m_kernelDirections[hash(rx1, ry0, rz0)];
    const vec3& d010 = m_kernelDirections[hash(rx0, ry1, rz0)];
    const vec3& d110 = m_kernelDirections[hash(rx1, ry1, rz0)];

    const vec3& d001 = m_kernelDirections[hash(rx0, ry0, rz1)];
    const vec3& d101 = m_kernelDirections[hash(rx1, ry0, rz1)];
    const vec3& d011 = m_kernelDirections[hash(rx0, ry1, rz1)];
    const vec3& d111 = m_kernelDirections[hash(rx1, ry1, rz1)];

    // generate vectors going from the grid points to p
    float x0 = tx, x1 = tx - 1;
    float y0 = ty, y1 = ty - 1;
    float z0 = tz, z1 = tz - 1;

    vec3 p000 = vec3(x0, y0, z0);
    vec3 p100 = vec3(x1, y0, z0);
    vec3 p010 = vec3(x0, y1, z0);
    vec3 p110 = vec3(x1, y1, z0);

    vec3 p001 = vec3(x0, y0, z1);
    vec3 p101 = vec3(x1, y0, z1);
    vec3 p011 = vec3(x0, y1, z1);
    vec3 p111 = vec3(x1, y1, z1);

    // remapping of tx and ty using the Smoothstep function 
    float sx = smoothstep(tx);
    float sy = smoothstep(ty);
    float sz = smoothstep(tz);

    float a = interpolate(dot(d000, p000), dot(d100, p100), sx);
    float b = interpolate(dot(d010, p010), dot(d110, p110), sx);
    float c = interpolate(dot(d001, p001), dot(d101, p101), sx);
    float d = interpolate(dot(d011, p011), dot(d111, p111), sx);

    float e = interpolate(a, b, sy);
    float f = interpolate(c, d, sy);

    // linearly interpolate the nx0/nx1 along they y axis
    return (interpolate(e, f, sz) + 1.0f) / 2.0f;
}

void PerlinNoise3D::computeKernelDirection()
{
    m_kernelDirections.clear();
    m_kernelDirections.resize(m_kernelSize);
    m_permutationTable.resize(2 * m_kernelSize);

    std::mt19937 generator(m_randomSeed);
    std::uniform_real_distribution<float> distribution;
    auto dice = std::bind(distribution, generator);

    for (unsigned i = 0; i < m_kernelSize; ++i) {
        // better
        float theta = acos(2 * dice() - 1);
        float phi = 2 * dice() * PI;
        
        float x = cos(phi) * sin(theta);
        float y = sin(phi) * sin(theta);
        float z = cos(theta);
        m_kernelDirections[i] = vec3(x, y, z);
        m_permutationTable[i] = i;
    }

    std::uniform_int_distribution<unsigned> distributionInt;
    auto diceInt = std::bind(distributionInt, generator);

    for (unsigned i = 0; i < m_kernelSize; ++i) {
        std::swap(m_permutationTable[i], m_permutationTable[diceInt() % m_kernelSize]);
        m_permutationTable[m_kernelSize + i] = m_permutationTable[i];
    }
}

float PerlinNoise3D::smoothstep(float val) const
{
    return val * val * (3.0f - 2.0f * val);
}

float PerlinNoise3D::interpolate(float min, float max, float value) const
{
    return min * (1.0f - value) + max * value;
}

int PerlinNoise3D::hash(int x, int y, int z) const
{
   return m_permutationTable[m_permutationTable[m_permutationTable[x] + y] + z];
}