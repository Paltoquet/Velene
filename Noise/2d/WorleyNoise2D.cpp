#include "WorleyNoise2D.h"

#include <cmath> 
#include <cstdio> 
#include <random> 
#include <functional> 

WorleyNoise2D::WorleyNoise2D(const IVector2& dimension, uint32_t nbSubdiv):
    m_dimension(dimension),
    m_nbSubDiv(nbSubdiv),
    m_rowHeight(m_dimension.getY() / m_nbSubDiv),
    m_colWidth(m_dimension.getX() / m_nbSubDiv),
    m_randomSeed(42)
{
    computeKernel();
}

WorleyNoise2D::~WorleyNoise2D()
{

}

/* --------------------------------- Public methods --------------------------------- */

std::vector<float> WorleyNoise2D::generateTexture()
{
    std::vector<float> result;
    int width = m_dimension.getX();
    int height = m_dimension.getY();
    float invX = 1.0f / width;
    float invY = 1.0f / height;

    result.resize(width * height);
    for (int y = 0; y < height; y++) {
        int row = y / m_rowHeight;
        float yoffset = (y - (row * m_rowHeight)) / float(m_rowHeight);
        for (int x = 0; x < width; x++) {
            int col = x / m_colWidth;
            float xoffset = (x - (col * m_colWidth)) / float(m_colWidth);
            float val = sample(row, col, vec2(xoffset, yoffset));
            result[y * width + x] = val;
        }
    }

    return result;
}

float WorleyNoise2D::evaluate(uint32_t x, uint32_t y)
{
    uint32_t row = y / m_rowHeight;
    uint32_t col = x / m_colWidth;
    float yoffset = (y - (row * m_rowHeight)) / float(m_rowHeight);
    float xoffset = (x - (col * m_colWidth)) / float(m_colWidth);
    return sample(row, col, vec2(xoffset, yoffset));
}

float WorleyNoise2D::sample(int row, int col, const vec2& offset)
{
    float minDist = 100.0f;

    // Sample 9 box around the currentPoint
    for (int y = -1; y <= 1; y++) {
        int sampledRow = row + y;
        sampledRow = sampledRow < 0 ? m_nbSubDiv - 1 : sampledRow;
        sampledRow = sampledRow >= m_nbSubDiv ? 0 : sampledRow;
        for (int x = -1; x <= 1; x++) {
            int sampledCol= col + x;
            sampledCol = sampledCol < 0 ? m_nbSubDiv - 1 : sampledCol;
            sampledCol = sampledCol >= m_nbSubDiv ? 0 : sampledCol;
            // current reference point in the [-1, 1] cube
            vec2 pointPos = vec2(float(x), float(y));
            pointPos += m_kernelData[sampledRow * m_nbSubDiv + sampledCol];
            float dist = Vectormath::length(pointPos - offset);
            if (dist < minDist) {
                minDist = dist;
            }
        }
    }

    minDist = min(minDist, 1.0f);
    return minDist;
}

/* --------------------------------- Private methods --------------------------------- */

void WorleyNoise2D::computeKernel()
{
    std::mt19937 gen(m_randomSeed);
    std::uniform_real_distribution<float> distrFloat(0, 1);
    auto randFloat = std::bind(distrFloat, gen);
    m_kernelData.resize(m_nbSubDiv * m_nbSubDiv);

    for (size_t y = 0; y < m_nbSubDiv; y++) {
        for (size_t x = 0; x < m_nbSubDiv; x++) {
            vec2 offset = vec2(randFloat(), randFloat());
            m_kernelData[y * m_nbSubDiv + x] = offset;
        }
    }
}