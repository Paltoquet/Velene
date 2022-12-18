#pragma once

#include <vector>
#include <string>
#include <cstdint>

struct Texture;

class ImageLoader
{
public:
    // -------- files
    static void saveOneChannel(const std::string& filename, const std::vector<float>& data, int width, int height);
    // -------- 2d
    static void genTexture(const std::vector<float>& data, int width, int height, Texture** pOutTexture);
    static void genBlueNoiseTexture(uint32_t width, uint32_t height, Texture** pOutTexture);
    static void genPerlinFBMTexture(uint32_t width, uint32_t height, Texture** pOutTexture);
    static void genWorleyFBMTexture(uint32_t width, uint32_t height, Texture** pOutTexture);
    static void genWeatherTexture(uint32_t width, uint32_t height, Texture** pOutTexture, float scale, int randomSeed);
    static void updateWeatherTexture(uint32_t width, uint32_t height, Texture** pOutTexture, float scale, int randomSeed);

    static void genTestTexture(uint32_t width, uint32_t height, std::vector<float>& data);

    // -------- 3d
    static void genCloudShapeTexture(uint32_t width, uint32_t height, uint32_t depth, Texture** pOutTexture, int randomSeed);
    static void updateCloudShapeTexture(uint32_t width, uint32_t height, uint32_t depth, Texture** pOutTexture, int randomSeed);
    static void gen3DNoiseTexture(uint32_t width, uint32_t height, uint32_t depth, Texture** pOutTexture, int randomSeed);
};

