#include "ImageLoader.h"
#include "../Noise/2d/WorleyNoise2D.h"
#include "../Noise/2d/PerlinNoise2D.h"
#include "../Noise/2d/BlueNoise2D.h"
#include "../Noise/3d/WorleyNoise3D.h"
#include "../Noise/3d/PerlinNoise3D.h"

#include "../../../../../Common_3/Utilities/ThirdParty/OpenSource/Nothings/stb_image_write.h"
#include "../../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"
#include "../../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../../Common_3/Utilities/Interfaces/IMemory.h"

/// Map a value from from the [l0-h0] to [l1-h1] range
float remap(float val, float l0, float h0, float l1, float h1)
{
	return l1 + (val - l0) * (h1 - l1) / (h0 - l0);
}

/* --------------------------------- Public methods --------------------------------- */

void ImageLoader::saveOneChannel(const std::string& filename, const std::vector<float>& data, int width, int height)
{
	char* imageData = (char*)tf_malloc(width * height * 3);
	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			int srcIndex = j * width + i;
			int dstIndex = 3 * srcIndex;
			unsigned char pixel = static_cast<unsigned char>(data[srcIndex] * 255);
			imageData[dstIndex] = pixel;
			imageData[dstIndex + 1] = pixel;
			imageData[dstIndex + 2] = pixel;
		}
	}
	stbi_write_png(filename.c_str(), width, height, 3, imageData, width * 3);
	
	tf_free(imageData);
}

/* --------------------------------- 2D Noise Texture --------------------------------- */

void ImageLoader::genTestTexture(uint32_t width, uint32_t height, std::vector<float>& data)
{

	data.reserve(width * height);

	IVector2 dim = IVector2(width, height);
	WorleyNoise2D worleyGenerator2D(dim, 3);
	WorleyNoise2D worleyGenerator2DFirst(dim, 6);
	WorleyNoise2D worleyGenerator2DSecond(dim, 12);
	WorleyNoise2D worleyGenerator2DThird(dim, 24);
	WorleyNoise2D worleyGenerator2DFourth(dim, 32);
	WorleyNoise2D worleyGenerator2DFifth(dim, 64);
	PerlinNoise2D perlinGenerator2D(dim, IVector2(64), 3, 1.0f, 42);
	PerlinNoise2D weatherGenerator2D(IVector2(width, height), IVector2(64, 64), 5, 0.1f, 42);


	float threshold = 0.2f;

	for (uint32_t y = 0; y < height; ++y)
	{
		for (uint32_t x = 0; x < width; ++x)
		{
			float worley = worleyGenerator2D.evaluate(x, y);
			worley = 1.0f - worley;
			float perlin = perlinGenerator2D.evaluate(x, y);
			// remap perlin with worley (ie: keep worley values when high)
			float c = remap(perlin, 0.0f, 1.0, worley, 1.0);

			float extrusionFactor = 0.5f * worleyGenerator2DFirst.evaluate(x, y)
				+ 0.25f * worleyGenerator2DSecond.evaluate(x, y)
				+ 0.175f * worleyGenerator2DThird.evaluate(x, y)
				+ 0.075f * worleyGenerator2DFourth.evaluate(x, y);

			float detail = 0.625f * worleyGenerator2DThird.evaluate(x, y)
				+ 0.25f * worleyGenerator2DFourth.evaluate(x, y)
				+ 0.125f * worleyGenerator2DFifth.evaluate(x, y);

			float afterExtrude = saturate(remap(c, 1.0f - extrusionFactor, 1.0f, 0.0f, 1.0f));

			float w = weatherGenerator2D.evaluate(x, y);
			w = max(w - threshold, 0.0f);
			w = min(1.0f, remap(w, 0.0f, 1.0f - threshold, 0.0f, 1.0f));
			w *= 1.6f;
			w = min(w, 1.0f);

			float afterWeather = saturate(remap(afterExtrude, 1.0f - w, 1.0f, 0.0f, 1.0f));

			float final = saturate(remap(afterWeather, detail, 1.0f, 0.0f, 1.0f));

			//afterWeather = afterExtrude * w;
			//detail = worleyGenerator3DFourth.evaluate(x, y, z);;

			int32_t cr = (int32_t)(c * 255.0f);
			int32_t cg = (int32_t)(extrusionFactor * 255.0f);
			int32_t cb = (int32_t)(detail * 255.0f);

			data[y * height + x] = perlin; // (cr) << 16 | (cr) << 8 | (cr) << 0;
		}
	}
}


void ImageLoader::genTexture(const std::vector<float>& data, int width, int height, Texture** pOutTexture)
{
	TextureDesc desc = {};
	desc.mArraySize = 1;
	desc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
	desc.mDepth = 1;
	desc.mWidth = width;
	desc.mHeight = height;
	desc.mMipLevels = 1;
	desc.mSampleCount = SAMPLE_COUNT_1;
	desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	desc.mStartState = RESOURCE_STATE_COMMON;
	TextureLoadDesc textureDesc = {};
	textureDesc.pDesc = &desc;
	textureDesc.ppTexture = pOutTexture;
	addResource(&textureDesc, NULL);

	uint32_t    slice = 0;
	TextureUpdateDesc updateDesc = {};
	updateDesc.pTexture = *pOutTexture;
	updateDesc.mArrayLayer = slice;
	beginUpdateResource(&updateDesc);

	for (size_t y = 0; y < updateDesc.mRowCount; ++y)
	{
		uint32_t* scanline = (uint32_t*)(updateDesc.pMappedData + (y * updateDesc.mDstRowStride));
		for (size_t x = 0; x < width; ++x)
		{
			float c = data[width * y + x];
			int32_t cr = (int32_t)(c * 255.0f);
			int32_t cg = (int32_t)(c * 255.0f);
			int32_t cb = (int32_t)(c * 255.0f);
			scanline[x] = (cb) << 16 | (cg) << 8 | (cr) << 0;
		}
	}

	endUpdateResource(&updateDesc, NULL);
}

void ImageLoader::genBlueNoiseTexture(uint32_t width, uint32_t height, Texture** pOutTexture)
{
	BlueNoise2D blueNoiseGenerator(IVector2(width, height));

	TextureDesc desc = {};
	desc.mArraySize = 1;
	desc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
	desc.mDepth = 1;
	desc.mWidth = width;
	desc.mHeight = height;
	desc.mMipLevels = 1;
	desc.mSampleCount = SAMPLE_COUNT_1;
	desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	desc.mStartState = RESOURCE_STATE_COMMON;
	TextureLoadDesc textureDesc = {};
	textureDesc.pDesc = &desc;
	textureDesc.ppTexture = pOutTexture;
	addResource(&textureDesc, NULL);

	uint32_t    slice = 0;
	TextureUpdateDesc updateDesc = {};
	updateDesc.pTexture = *pOutTexture;
	updateDesc.mArrayLayer = slice;
	beginUpdateResource(&updateDesc);

	for (uint32_t y = 0; y < updateDesc.mRowCount; ++y)
	{
		uint32_t* scanline = (uint32_t*)(updateDesc.pMappedData + (y * updateDesc.mDstRowStride));
		for (uint32_t x = 0; x < width; ++x)
		{
			float c = blueNoiseGenerator.evaluate(x, y);
			int32_t cr = (int32_t)(c * 255.0f);
			int32_t cg = (int32_t)(c * 255.0f);
			int32_t cb = (int32_t)(c * 255.0f);
			scanline[x] = (cb) << 16 | (cg) << 8 | (cr) << 0;
		}
	}

	endUpdateResource(&updateDesc, NULL);
}

void ImageLoader::genPerlinFBMTexture(uint32_t width, uint32_t height, Texture** pOutTexture)
{
	PerlinNoise2D perlinGenerator(IVector2(width, height), IVector2(64, 64), 3, 1.0f, 42);

	TextureDesc desc = {};
	desc.mArraySize = 1;
	desc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
	desc.mDepth = 1;
	desc.mWidth = width;
	desc.mHeight = height;
	desc.mMipLevels = 1;
	desc.mSampleCount = SAMPLE_COUNT_1;
	desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	desc.mStartState = RESOURCE_STATE_COMMON;
	TextureLoadDesc textureDesc = {};
	textureDesc.pDesc = &desc;
	textureDesc.ppTexture = pOutTexture;
	addResource(&textureDesc, NULL);

	uint32_t    slice = 0;
	TextureUpdateDesc updateDesc = {};
	updateDesc.pTexture = *pOutTexture;
	updateDesc.mArrayLayer = slice;
	beginUpdateResource(&updateDesc);

	for (uint32_t y = 0; y < updateDesc.mRowCount; ++y)
	{
		uint32_t* scanline = (uint32_t*)(updateDesc.pMappedData + (y * updateDesc.mDstRowStride));
		for (uint32_t x = 0; x < width; ++x)
		{
			float c = perlinGenerator.evaluate(x, y);
			int32_t cr = (int32_t)(c * 255.0f);
			int32_t cg = (int32_t)(c * 255.0f);
			int32_t cb = (int32_t)(c * 255.0f);
			scanline[x] = (cb) << 16 | (cg) << 8 | (cr) << 0;
		}
	}

	endUpdateResource(&updateDesc, NULL);
}

void ImageLoader::genWorleyFBMTexture(uint32_t width, uint32_t height, Texture** pOutTexture)
{
	WorleyNoise2D firstWorleyGenerator(IVector2(width, height), 3);
	WorleyNoise2D secondWorleyGenerator(IVector2(width, height), 6);
	WorleyNoise2D thirdWorleyGenerator(IVector2(width, height), 12);

	TextureDesc desc = {};
	desc.mArraySize = 1;
	desc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
	desc.mDepth = 1;
	desc.mWidth = width;
	desc.mHeight = height;
	desc.mMipLevels = 1;
	desc.mSampleCount = SAMPLE_COUNT_1;
	desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	desc.mStartState = RESOURCE_STATE_COMMON;
	TextureLoadDesc textureDesc = {};
	textureDesc.pDesc = &desc;
	textureDesc.ppTexture = pOutTexture;
	addResource(&textureDesc, NULL);

	uint32_t    slice = 0;
	TextureUpdateDesc updateDesc = {};
	updateDesc.pTexture = *pOutTexture;
	updateDesc.mArrayLayer = slice;
	beginUpdateResource(&updateDesc);

	for (uint32_t y = 0; y < updateDesc.mRowCount; ++y)
	{
		uint32_t* scanline = (uint32_t*)(updateDesc.pMappedData + (y * updateDesc.mDstRowStride));
		for (uint32_t x = 0; x < width; ++x)
		{
			float c = 0.625f * firstWorleyGenerator.evaluate(x, y)
				+ 0.25f * secondWorleyGenerator.evaluate(x, y)
				+ 0.125f * thirdWorleyGenerator.evaluate(x, y);
			// invert worley noise
			c = 1.0f - c;

			int32_t cr = (int32_t)(c * 255.0f);
			int32_t cg = (int32_t)(c * 255.0f);
			int32_t cb = (int32_t)(c * 255.0f);
			scanline[x] = (cb) << 16 | (cg) << 8 | (cr) << 0;
		}
	}

	endUpdateResource(&updateDesc, NULL);
}

void ImageLoader::genWeatherTexture(uint32_t width, uint32_t height, Texture** pOutTexture, float scale, int randomSeed)
{
	PerlinNoise2D perlinGenerator(IVector2(width, height), IVector2(64, 64), 5, scale, randomSeed);

	TextureDesc desc = {};
	desc.mArraySize = 1;
	desc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
	desc.mDepth = 1;
	desc.mWidth = width;
	desc.mHeight = height;
	desc.mMipLevels = 1;
	desc.mSampleCount = SAMPLE_COUNT_1;
	desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	desc.mStartState = RESOURCE_STATE_COMMON;
	TextureLoadDesc textureDesc = {};
	textureDesc.pDesc = &desc;
	textureDesc.ppTexture = pOutTexture;
	addResource(&textureDesc, NULL);

	updateWeatherTexture(width, height, pOutTexture, scale, randomSeed);
}

void ImageLoader::updateWeatherTexture(uint32_t width, uint32_t height, Texture** pOutTexture, float scale, int randomSeed)
{
	PerlinNoise2D perlinGenerator(IVector2(width, height), IVector2(64, 64), 5, scale, randomSeed);

	uint32_t    slice = 0;
	TextureUpdateDesc updateDesc = {};
	updateDesc.pTexture = *pOutTexture;
	updateDesc.mArrayLayer = slice;
	beginUpdateResource(&updateDesc);

	float threshold = 0.2f;
	float tresholdLimit = 1.0f - threshold;
	for (uint32_t y = 0; y < updateDesc.mRowCount; ++y)
	{
		uint32_t* scanline = (uint32_t*)(updateDesc.pMappedData + (y * updateDesc.mDstRowStride));
		for (uint32_t x = 0; x < width; ++x)
		{
			float c = perlinGenerator.evaluate(x, y);
			c = max(c - threshold, 0.0f);
			c = min(1.0f, remap(c, 0.0f, 1.0f - threshold, 0.0f, 1.0f));
			//c = 1.0f - c;

			int32_t cr = (int32_t)(c * 255.0f);
			int32_t cg = (int32_t)(c * 255.0f);
			int32_t cb = (int32_t)(c * 255.0f);
			scanline[x] = (cb) << 16 | (cg) << 8 | (cr) << 0;
		}
	}

	endUpdateResource(&updateDesc, NULL);
}

/* --------------------------------- 3D Noise Texture --------------------------------- */

void ImageLoader::gen3DNoiseTexture(uint32_t width, uint32_t height, uint32_t depth, Texture** pOutTexture, int randomSeed)
{
	WorleyNoise3D firstWorleyGenerator(IVector3(width, height, depth), 3, randomSeed);
	WorleyNoise3D secondWorleyGenerator(IVector3(width, height, depth), 6, randomSeed);
	WorleyNoise3D thirdWorleyGenerator(IVector3(width, height, depth), 12, randomSeed);

	TextureDesc desc = {};
	desc.mArraySize = 1;
	desc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
	//desc.mFlags = TextureCreationFlags::TEXTURE_CREATION_FLAG_FORCE_3D;
	desc.mWidth = width;
	desc.mHeight = height;
	desc.mDepth = depth;
	desc.mMipLevels = 1;
	desc.mSampleCount = SAMPLE_COUNT_1;
	desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	desc.mStartState = RESOURCE_STATE_COMMON;
	TextureLoadDesc textureDesc = {};
	textureDesc.pDesc = &desc;
	textureDesc.ppTexture = pOutTexture;
	addResource(&textureDesc, NULL);

	uint32_t    layer = 0;
	TextureUpdateDesc updateDesc = {};
	updateDesc.pTexture = *pOutTexture;
	updateDesc.mArrayLayer = layer;
	beginUpdateResource(&updateDesc);

	for (uint32_t z = 0; z < depth; ++z) 
	{
		uint32_t* dstSliceData = (uint32_t*)updateDesc.pMappedData + updateDesc.mDstSliceStride * z;

		float test = (z / float(depth));

		for (uint32_t y = 0; y < updateDesc.mRowCount; ++y)
		{
			//uint32_t* scanline = (uint32_t*)(dstSliceData + (y * updateDesc.mDstRowStride));
			uint32_t* scanline = (uint32_t*)(updateDesc.pMappedData + updateDesc.mDstSliceStride * z + (y * updateDesc.mDstRowStride));
			for (uint32_t x = 0; x < width; ++x)
			{
				float c = 0.625f * firstWorleyGenerator.evaluate(x, y, z)
					+ 0.25f * secondWorleyGenerator.evaluate(x, y, z)
					+ 0.125f * thirdWorleyGenerator.evaluate(x, y, z);
				// invert worley noise
				c = 1.0f - c;

				int32_t cr = (int32_t)(c * 255.0f);
				int32_t cg = (int32_t)(c * 255.0f);
				int32_t cb = (int32_t)(c * 255.0f);
				scanline[x] = (cb) << 16 | (cg) << 8 | (cr) << 0;
			}
		}
	}

	endUpdateResource(&updateDesc, NULL);
}

void ImageLoader::genCloudShapeTexture(uint32_t width, uint32_t height, uint32_t depth, Texture** pOutTexture, int randomSeed)
{
	TextureDesc desc = {};
	desc.mArraySize = 1;
	desc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
	//desc.mFlags = TextureCreationFlags::TEXTURE_CREATION_FLAG_FORCE_3D;
	desc.mWidth = width;
	desc.mHeight = height;
	desc.mDepth = depth;
	desc.mMipLevels = 1;
	desc.mSampleCount = SAMPLE_COUNT_1;
	desc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
	desc.mStartState = RESOURCE_STATE_COMMON;
	TextureLoadDesc textureDesc = {};
	textureDesc.pDesc = &desc;
	textureDesc.ppTexture = pOutTexture;
	addResource(&textureDesc, NULL);

	updateCloudShapeTexture(width, height, depth, pOutTexture, randomSeed);
}

void ImageLoader::updateCloudShapeTexture(uint32_t width, uint32_t height, uint32_t depth, Texture** pOutTexture, int randomSeed)
{
	IVector3 dim = IVector3(width, height, depth);
	WorleyNoise3D worleyGenerator3D(dim, 3, randomSeed);
	WorleyNoise3D worleyGenerator3DFirst(dim, 6, randomSeed);
	WorleyNoise3D worleyGenerator3DSecond(dim, 12, randomSeed);
	WorleyNoise3D worleyGenerator3DThird(dim, 24, randomSeed);
	WorleyNoise3D worleyGenerator3DFourth(dim, 32, randomSeed);
	WorleyNoise3D worleyGenerator3DFifth(dim, 64, randomSeed);
	PerlinNoise3D perlinGenerator3D(dim, 64, 3, 1.0f, randomSeed);

	uint32_t    layer = 0;
	TextureUpdateDesc updateDesc = {};
	updateDesc.pTexture = *pOutTexture;
	updateDesc.mArrayLayer = layer;
	beginUpdateResource(&updateDesc);

	for (uint32_t z = 0; z < depth; ++z)
	{
		uint32_t* dstSliceData = (uint32_t*)updateDesc.pMappedData + updateDesc.mDstSliceStride * z;

		float test = (z / float(depth));

		for (uint32_t y = 0; y < updateDesc.mRowCount; ++y)
		{
			//uint32_t* scanline = (uint32_t*)(dstSliceData + (y * updateDesc.mDstRowStride));
			uint32_t* scanline = (uint32_t*)(updateDesc.pMappedData + updateDesc.mDstSliceStride * z + (y * updateDesc.mDstRowStride));
			for (uint32_t x = 0; x < width; ++x)
			{
				float worley = worleyGenerator3D.evaluate(x, y, z);
				worley = 1.0f - worley;
				float perlin = perlinGenerator3D.evaluate(x, y, z);
				// remap perlin with worley (ie: keep worley values when high)
				float c = remap(perlin, 0.0f, 1.0, worley, 1.0);

				float extrusionFactor = 0.5f * worleyGenerator3DFirst.evaluate(x, y, z)
					+ 0.25f * worleyGenerator3DSecond.evaluate(x, y, z)
					+ 0.175f * worleyGenerator3DThird.evaluate(x, y, z)
					+ 0.075f * worleyGenerator3DFourth.evaluate(x, y, z);

				float detail = 0.625f * worleyGenerator3DThird.evaluate(x, y, z)
					+ 0.25f * worleyGenerator3DFourth.evaluate(x, y, z)
					+ 0.125f * worleyGenerator3DFifth.evaluate(x, y, z);

				//detail = worleyGenerator3DFourth.evaluate(x, y, z);;

				int32_t cr = (int32_t)(c * 255.0f);
				int32_t cg = (int32_t)(extrusionFactor * 255.0f);
				int32_t cb = (int32_t)(detail * 255.0f);
				scanline[x] = (cb) << 16 | (cg) << 8 | (cr) << 0;
			}
		}
	}

	endUpdateResource(&updateDesc, NULL);
}

