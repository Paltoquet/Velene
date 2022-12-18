#include "WorleyNoise3D.h"

#include "../../../../../Common_3/Utilities/ThirdParty/OpenSource/EASTL/vector.h"

#include <cmath> 
#include <cstdio> 
#include <random> 
#include <functional> 

WorleyNoise3D::WorleyNoise3D(const IVector3& dimension, uint32_t nbSubdiv, int randomSeed):
    m_dimension(dimension),
    m_nbSubDiv(nbSubdiv),
    m_sliceDepth(m_dimension.getZ() / m_nbSubDiv),
    m_rowHeight(m_dimension.getY() / m_nbSubDiv),
    m_colWidth(m_dimension.getX() / m_nbSubDiv),
    m_randomSeed(randomSeed)
{
    computeKernel();
}

WorleyNoise3D::~WorleyNoise3D()
{

}

/* --------------------------------- Public methods --------------------------------- */

std::vector<float> WorleyNoise3D::generateTexture()
{
    std::vector<float> result;
    int width = m_dimension.getX();
    int height = m_dimension.getY();
    int depth = m_dimension.getZ();
    float invX = 1.0f / width;
    float invY = 1.0f / height;
    float invZ = 1.0f / depth;
    int32_t pageSize = height * width;

    result.resize(depth * width * height);
    for (int z = 0; z < depth; z++) {
        int slice = z / m_sliceDepth;
        float zOffset = (z - (slice * m_sliceDepth)) / float(m_sliceDepth);

        for (int y = 0; y < height; y++) {
            int row = y / m_rowHeight;
            float yoffset = (y - (row * m_rowHeight)) / float(m_rowHeight);

            for (int x = 0; x < width; x++) {
                int col = x / m_colWidth;
                float xoffset = (x - (col * m_colWidth)) / float(m_colWidth);
                float val = sample(slice, row, col, vec3(xoffset, yoffset, zOffset));
                result[z * pageSize + y * width + x] = val;
            }
        }
    }

    return result;
}

float WorleyNoise3D::evaluate(uint32_t x, uint32_t y, uint32_t z)
{
    uint32_t slice = z / m_sliceDepth;
    uint32_t row = y / m_rowHeight;
    uint32_t col = x / m_colWidth;
    float zoffset = (z - (slice * m_sliceDepth)) / float(m_sliceDepth);
    float yoffset = (y - (row * m_rowHeight)) / float(m_rowHeight);
    float xoffset = (x - (col * m_colWidth)) / float(m_colWidth);
    return sample(slice, row, col, vec3(xoffset, yoffset, zoffset));
}

float WorleyNoise3D::sample(int slice, int row, int col, const vec3& offset)
{
    float minDist = 100.0f;
    int32_t kernelPageSize = m_nbSubDiv * m_nbSubDiv;

    // Sample 27 box around the currentPoint
    for (int z = -1; z <= 1; z++) {
        int sampledSlice = slice + z;
        sampledSlice = sampledSlice < 0 ? m_nbSubDiv - 1 : sampledSlice;
        sampledSlice = sampledSlice >= m_nbSubDiv ? 0 : sampledSlice;

        for (int y = -1; y <= 1; y++) {
            int sampledRow = row + y;
            sampledRow = sampledRow < 0 ? m_nbSubDiv - 1 : sampledRow;
            sampledRow = sampledRow >= m_nbSubDiv ? 0 : sampledRow;

            for (int x = -1; x <= 1; x++) {
                int sampledCol = col + x;
                sampledCol = sampledCol < 0 ? m_nbSubDiv - 1 : sampledCol;
                sampledCol = sampledCol >= m_nbSubDiv ? 0 : sampledCol;
                // current reference point in the [-1, 1] cube
                vec3 pointPos = vec3(float(x), float(y), float(z));
                pointPos += m_kernelData[sampledSlice * kernelPageSize + sampledRow * m_nbSubDiv + sampledCol];
                float dist = length(pointPos - offset);
                if (dist < minDist) {
                    minDist = dist;
                }
            }
        }
    }

    minDist = min(minDist, 1.0f);
    return minDist;
}

/* --------------------------------- Private methods --------------------------------- */

void WorleyNoise3D::computeKernel()
{

    std::mt19937 gen(m_randomSeed);
    std::uniform_real_distribution<float> distrFloat(0, 1);
    auto randFloat = std::bind(distrFloat, gen);
    m_kernelData.resize(m_nbSubDiv * m_nbSubDiv * m_nbSubDiv);
    int32_t kernelPageSize = m_nbSubDiv * m_nbSubDiv;

    for (int32_t z = 0; z < m_nbSubDiv; z++) {
        for (int32_t y = 0; y < m_nbSubDiv; y++) {
            for (int32_t x = 0; x < m_nbSubDiv; x++) {
                vec3 offset = vec3(randFloat(), randFloat(), randFloat());
                m_kernelData[z * kernelPageSize + y * m_nbSubDiv + x] = offset;
            }
        }
    }
}