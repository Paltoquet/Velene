/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

// Unit Test for testing transformations using a solar system.
// Tests the basic mat4 transformations, such as scaling, rotation, and translation.

#include "Noise/3d/WorleyNoise3D.h"
#include "Noise/3d/PerlinNoise3D.h"
#include "Noise/2d/WorleyNoise2D.h"
#include "Noise/2d/ValueNoise2D.h"
#include "Noise/2d/PerlinNoise2D.h"
#include "Noise/2d/BlueNoise2D.h"
#include "Utils/ImageLoader.h"

#include <random> 
#include <functional> 

//Interfaces
#include "../../../../Common_3/Application/Interfaces/ICameraController.h"
#include "../../../../Common_3/Application/Interfaces/IApp.h"
#include "../../../../Common_3/Utilities/Interfaces/ILog.h"
#include "../../../../Common_3/Application/Interfaces/IInput.h"
#include "../../../../Common_3/Utilities/Interfaces/IFileSystem.h"
#include "../../../../Common_3/Utilities/Interfaces/ITime.h"
#include "../../../../Common_3/Application/Interfaces/IProfiler.h"
#include "../../../../Common_3/Application/Interfaces/IScreenshot.h"
#include "../../../../Common_3/Game/Interfaces/IScripting.h"
#include "../../../../Common_3/Application/Interfaces/IFont.h"
#include "../../../../Common_3/Application/Interfaces/IUI.h"

//Renderer
#include "../../../../Common_3/Graphics/Interfaces/IGraphics.h"
#include "../../../../Common_3/Resources/ResourceLoader/Interfaces/IResourceLoader.h"

//Math
#include "../../../../Common_3/Utilities/Math/MathTypes.h"
#include "../../../../Common_3/Utilities/Interfaces/IMemory.h"

struct UniformBlock
{
	CameraMatrix mProjectView;
	mat4 mToWorldMat;

	// Point Light Information
	vec3 mCameraPos;
};

struct ViewParams {

};

const uint32_t gImageCount = 3;

Renderer* pRenderer = NULL;

Queue*   pGraphicsQueue = NULL;
CmdPool* pCmdPools[gImageCount] = { NULL };
Cmd*     pCmds[gImageCount] = { NULL };

SwapChain*    pSwapChain = NULL;
RenderTarget* pDepthBuffer = NULL;
Fence*        pRenderCompleteFences[gImageCount] = { NULL };
Semaphore*    pImageAcquiredSemaphore = NULL;
Semaphore*    pRenderCompleteSemaphores[gImageCount] = { NULL };

Shader* pQuadDrawShader = NULL;
Buffer* pQuadVertexBuffer = NULL;
Pipeline* pQuadDrawPipeline = NULL;

RootSignature* pRootSignature = NULL;
Sampler*       pSamplerQuad = NULL;
Sampler*       pSamplerCloud = NULL;

//Texture*       pCloudShapeTexture;
//Texture*       pWeatherTexture;
//Texture*       pBlueNoiseTexture;

DescriptorSet* pDescriptorSetTexture = { NULL };
DescriptorSet* pDescriptorSetUniforms = { NULL };
//DescriptorSet* pDescriptorSetCloudData = { NULL };

Buffer* pProjViewUniformBuffer[gImageCount] = { NULL };

uint32_t gFrameIndex = 0;
ProfileToken gGpuProfileToken = PROFILE_INVALID_TOKEN;

UniformBlock     gUniformData;
ViewParams       pViewParams;

ICameraController* pCameraController = NULL;

UIComponent*    pGuiWindow = NULL;
UIComponent*	pTextureInterface = NULL;

uint32_t gFontID = 0; 

/// Breadcrumb
/* Markers to be used to pinpoint which command has caused GPU hang.
 * In this example, two markers get injected into the command list.
 * Pressing the crash button would result in a gpu hang.
 * In this sitatuion, the first marker would be written before the draw command, but the second one would stall for the draw command to finish.
 * Due to the infinite loop in the shader, the second marker won't be written, and we can reason that the draw command has caused the GPU hang.
 * We log the markers information to verify this. */
bool bHasCrashed = false;
uint32_t gCrashedFrame = 0;
const uint32_t gMarkerCount = 2;
const uint32_t gValidMarkerValue = 1U;

Buffer* pMarkerBuffer[gImageCount] = { NULL };

DECLARE_RENDERER_FUNCTION(void, mapBuffer, Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
DECLARE_RENDERER_FUNCTION(void, unmapBuffer, Renderer* pRenderer, Buffer* pBuffer)

FontDrawDesc gFrameTimeDraw; 

const float quadPoints[] = {
	// z positive							/* uv */	
	-1.0f,   1.0f,   1.0f,					0.0f, 0.0f,
	 1.0f,  -1.0f,   1.0f,					1.0f, 1.0f,
	 1.0f,   1.0f,   1.0f,					1.0f, 0.0f,
			 
	-1.0f,   1.0f,   1.0f,					0.0f, 0.0f,
	-1.0f,  -1.0f,   1.0f,					0.0f, 1.0f,
	 1.0f,  -1.0f,   1.0f,					1.0f, 1.0f,
};

bool gTakeScreenshot = false;
void takeScreenshot(void* pUserData) 
{
	if (!gTakeScreenshot)
		gTakeScreenshot = true;
}

const char* gWindowTestScripts[] = 
{ 
	"TestFullScreen.lua", 
	"TestCenteredWindow.lua",
	"TestNonCenteredWindow.lua", 
	"TestBorderless.lua",
	"TestHideWindow.lua" 
};

class Transformations: public IApp
{
public:

	bool Init()
	{
		// FILE PATHS
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_SOURCES, "Shaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SHADER_BINARIES, "CompiledShaders");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_GPU_CONFIG, "GPUCfg");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_TEXTURES, "Textures");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_FONTS, "Fonts");
		fsSetPathForResourceDir(pSystemFileIO, RM_DEBUG, RD_SCREENSHOTS, "Screenshots");
		fsSetPathForResourceDir(pSystemFileIO, RM_CONTENT, RD_SCRIPTS, "Scripts");

		// window and renderer setup
		RendererDesc settings;
		memset(&settings, 0, sizeof(settings));
		settings.mD3D11Supported = true;
		settings.mGLESSupported = false;
		initRenderer(GetName(), &settings, &pRenderer);
		//check for init success
		if (!pRenderer)
			return false;

		QueueDesc queueDesc = {};
		queueDesc.mType = QUEUE_TYPE_GRAPHICS;
		queueDesc.mFlag = QUEUE_FLAG_INIT_MICROPROFILE;
		addQueue(pRenderer, &queueDesc, &pGraphicsQueue);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			CmdPoolDesc cmdPoolDesc = {};
			cmdPoolDesc.pQueue = pGraphicsQueue;
			addCmdPool(pRenderer, &cmdPoolDesc, &pCmdPools[i]);
			CmdDesc cmdDesc = {};
			cmdDesc.pPool = pCmdPools[i];
			addCmd(pRenderer, &cmdDesc, &pCmds[i]);

			addFence(pRenderer, &pRenderCompleteFences[i]);
			addSemaphore(pRenderer, &pRenderCompleteSemaphores[i]);
		}
		addSemaphore(pRenderer, &pImageAcquiredSemaphore);

		initScreenshotInterface(pRenderer, pGraphicsQueue);

		initResourceLoaderInterface(pRenderer);

		// Load quad Textures
		//WorleyNoise2D worleyGenerator = WorleyNoise2D(IVector2(128, 128), 8);
		//std::vector<float> worleyData = worleyGenerator.generateTexture();
		//std::string worleyFileName = std::string("G:\\Projects\\Velene\\worley.png");
		//ImageLoader::saveOneChannel(worleyFileName, worleyData, 128, 128);
		//
		//PerlinNoise2D perlinGenerator = PerlinNoise2D(IVector2(128, 128), IVector2(64, 64), 3, 3.0f);
		//std::vector<float> perlinData = perlinGenerator.generateTexture();
		//std::string perlinFileName = std::string("G:\\Projects\\Velene\\perlinx3.png");
		//ImageLoader::saveOneChannel(perlinFileName, perlinData, 128, 128);
		//
		//ValueNoise2D valueGenerator = ValueNoise2D(IVector2(128, 128), IVector2(64, 64), 3, 3.0f);
		//std::vector<float> valueData = valueGenerator.generateTexture();
		//std::string valueFileName = std::string("G:\\Projects\\Velene\\valuex3.png");
		//ImageLoader::saveOneChannel(valueFileName, valueData, 128, 128);

		//WorleyNoise3D worleyGenerator3D = WorleyNoise3D(IVector3(48, 48, 48), 4);
		//std::vector<float> valueData = worleyGenerator3D.generateTexture();
		//std::string valueFileName = std::string("G:\\Projects\\Velene\\worley3d.png");
		//ImageLoader::saveOneChannel(valueFileName, valueData, 48, 2304);

		//PerlinNoise3D perlinGenerator3D = PerlinNoise3D(IVector3(48, 48, 48), 64, 3, 1.0f);
		//std::vector<float> valueData = perlinGenerator3D.generateTexture();
		//std::string valueFileName = std::string("G:\\Projects\\Velene\\perlin3d.png");
		//ImageLoader::saveOneChannel(valueFileName, valueData, 48, 2304);

		//std::vector<float> data;
		//ImageLoader::genTestTexture(256, 256, data);
		////std::string valueFileName = std::string("G:\\Projects\\Velene\\perlin3d.png");
		//std::string valueFileName = std::string("C:\\Users\\thibault\\Documents\\velene\\perlin.png");
		//ImageLoader::saveOneChannel(valueFileName, data, 256, 256);

		//ImageLoader::genWeatherTexture(512, 512, &pWeatherTexture, pViewParams.weatherScale, pViewParams.randomSeed);
		//ImageLoader::genBlueNoiseTexture(128, 128, &pBlueNoiseTexture);
		//ImageLoader::genCloudShapeTexture(256, 256, 64, &pCloudShapeTexture, pViewParams.randomSeed);

		SamplerDesc quadSamplerDesc = { FILTER_LINEAR,
									FILTER_LINEAR,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE,
									ADDRESS_MODE_CLAMP_TO_EDGE };
		addSampler(pRenderer, &quadSamplerDesc, &pSamplerQuad);

		SamplerDesc cloudSamplerDesc = { FILTER_LINEAR,
									FILTER_LINEAR,
									MIPMAP_MODE_NEAREST,
									ADDRESS_MODE_REPEAT,
									ADDRESS_MODE_REPEAT,
									ADDRESS_MODE_REPEAT };
		addSampler(pRenderer, &cloudSamplerDesc, &pSamplerCloud);

		BufferLoadDesc ubDesc = {};
		ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
		ubDesc.mDesc.mSize = sizeof(UniformBlock);
		ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
		ubDesc.pData = NULL;
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			ubDesc.ppBuffer = &pProjViewUniformBuffer[i];
			addResource(&ubDesc, NULL);
		}

		// Quad Vertex Buffer
		uint64_t       quadDatasize = 5 * 6 * sizeof(float);
		BufferLoadDesc quadDesc = {};
		quadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
		quadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
		quadDesc.mDesc.mSize = quadDatasize;
		quadDesc.pData = quadPoints;
		quadDesc.ppBuffer = &pQuadVertexBuffer;
		addResource(&quadDesc, NULL);

		if (pRenderer->pActiveGpuSettings->mGpuBreadcrumbs)
		{
			// Initialize breadcrumb buffer to write markers in it.
			initMarkers();
		}

		// Load fonts
		FontDesc font = {};
		font.pFontPath = "TitilliumText/TitilliumText-Bold.otf";
		fntDefineFonts(&font, 1, &gFontID);

		FontSystemDesc fontRenderDesc = {};
		fontRenderDesc.pRenderer = pRenderer;
		if (!initFontSystem(&fontRenderDesc))
			return false; // report?

		// Initialize Forge User Interface Rendering
		UserInterfaceDesc uiRenderDesc = {};
		uiRenderDesc.pRenderer = pRenderer;
		initUserInterface(&uiRenderDesc);

		// Initialize micro profiler and its UI.
		ProfilerDesc profiler = {};
		profiler.pRenderer = pRenderer;
		profiler.mWidthUI = mSettings.mWidth;
		profiler.mHeightUI = mSettings.mHeight;
		initProfiler(&profiler);

		// Gpu profiler can only be added after initProfile.
		gGpuProfileToken = addGpuProfiler(pRenderer, pGraphicsQueue, "Graphics");

		/************************************************************************/
		// GUI
		/************************************************************************/
		UIComponentDesc guiDesc = {};
		guiDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.01f);
		uiCreateComponent(GetName(), &guiDesc, &pGuiWindow);

		UIComponentDesc textureDesc = {};
		textureDesc.mStartPosition = vec2(mSettings.mWidth * 0.01f, mSettings.mHeight * 0.02f);
		uiCreateComponent(GetName(), &textureDesc, &pTextureInterface);

		// Take a screenshot with a button.
		ButtonWidget screenshot;
		UIWidget* pScreenshot = uiCreateComponentWidget(pGuiWindow, "Screenshot", &screenshot, WIDGET_TYPE_BUTTON);
		uiSetWidgetOnEditedCallback(pScreenshot, nullptr, takeScreenshot);
		REGISTER_LUA_WIDGET(pScreenshot);

		/* --------------------- Initial Parameters --------------------- */

		//pViewParams.sunColor = float4(0.7f, 0.8f, 0.92f, 1.0f);

		/* --------------------- Light Parameters --------------------- */

		//SliderFloatWidget cloudAbsorptionSlider;
		//cloudAbsorptionSlider.pData = &pViewParams.lightAbsorption;
		//cloudAbsorptionSlider.mMin = 0.0f;
		//cloudAbsorptionSlider.mMax = 0.5f;
		//cloudAbsorptionSlider.mStep = 0.002f;
		//UIWidget* pAbsorbtionWidget = uiCreateComponentWidget(pGuiWindow, "Light Absorption", &cloudAbsorptionSlider, WIDGET_TYPE_SLIDER_FLOAT);

		const uint32_t numScripts = sizeof(gWindowTestScripts) / sizeof(gWindowTestScripts[0]);
		LuaScriptDesc scriptDescs[numScripts] = {};
		for (uint32_t i = 0; i < numScripts; ++i)
			scriptDescs[i].pScriptFileName = gWindowTestScripts[i];
		DEFINE_LUA_SCRIPTS(scriptDescs, numScripts);

		waitForAllResourceLoads();

		CameraMotionParameters cmp{480.0f, 1200.0f, 400.0f};
		vec3                   camPos{0.0f, 0.0f, -1.0f};
		vec3                   lookAt{vec3(0.0f, 0.0f, 1.0f)};

		pCameraController = initFpsCameraController(camPos, lookAt);
		pCameraController->setMotionParameters(cmp);

		InputSystemDesc inputDesc = {};
		inputDesc.pRenderer = pRenderer;
		inputDesc.pWindow = pWindow;
		if (!initInputSystem(&inputDesc))
			return false;

		// App Actions
		InputActionDesc actionDesc = {DefaultInputActions::DUMP_PROFILE_DATA, [](InputActionContext* ctx) {  dumpProfileData(((Renderer*)ctx->pUserData)->pName); return true; }, pRenderer};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TOGGLE_FULLSCREEN, [](InputActionContext* ctx) { toggleFullscreen(((IApp*)ctx->pUserData)->pWindow); return true; }, this};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::EXIT, [](InputActionContext* ctx) { requestShutdown(); return true; }};
		addInputAction(&actionDesc);
		InputActionCallback onAnyInput = [](InputActionContext* ctx)
		{
			if (ctx->mActionId > UISystemInputActions::UI_ACTION_START_ID_)
			{
				uiOnInput(ctx->mActionId, ctx->mBool, ctx->pPosition, &ctx->mFloat2);
			}

			return true;
		};

		typedef bool(*CameraInputHandler)(InputActionContext* ctx, uint32_t index);
		static CameraInputHandler onCameraInput = [](InputActionContext* ctx, uint32_t index)
		{
			if (*(ctx->pCaptured))
			{
				float2 delta = uiIsFocused() ? float2(0.f, 0.f) : ctx->mFloat2;
				index ? pCameraController->onRotate(delta) : pCameraController->onMove(delta);
			}
			return true;
		};
		actionDesc = {DefaultInputActions::CAPTURE_INPUT, [](InputActionContext* ctx) {setEnableCaptureInput(!uiIsFocused() && INPUT_ACTION_PHASE_CANCELED != ctx->mPhase);	return true;}, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::ROTATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 1); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::TRANSLATE_CAMERA, [](InputActionContext* ctx) { return onCameraInput(ctx, 0); }, NULL};
		addInputAction(&actionDesc);
		actionDesc = {DefaultInputActions::RESET_CAMERA, [](InputActionContext* ctx) { if (!uiWantTextInput()) pCameraController->resetView(); return true; }};
		addInputAction(&actionDesc);		
		GlobalInputActionDesc globalInputActionDesc = {GlobalInputActionDesc::ANY_BUTTON_ACTION, onAnyInput, this };
		setGlobalInputAction(&globalInputActionDesc);

		gFrameIndex = 0; 

		return true;
	}

	void Exit()
	{
		exitInputSystem();

		exitCameraController(pCameraController);

		exitUserInterface();

		exitFontSystem();

		// Exit profile
		exitProfiler();

		if (pRenderer->pActiveGpuSettings->mGpuBreadcrumbs)
		{
			exitMarkers();
		}

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pProjViewUniformBuffer[i]);
		}
		removeResource(pQuadVertexBuffer);

		removeSampler(pRenderer, pSamplerQuad);
		removeSampler(pRenderer, pSamplerCloud);

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeFence(pRenderer, pRenderCompleteFences[i]);
			removeSemaphore(pRenderer, pRenderCompleteSemaphores[i]);

			removeCmd(pRenderer, pCmds[i]);
			removeCmdPool(pRenderer, pCmdPools[i]);
		}
		removeSemaphore(pRenderer, pImageAcquiredSemaphore);

		exitResourceLoaderInterface(pRenderer);
		exitScreenshotInterface();

		removeQueue(pRenderer, pGraphicsQueue);

		exitRenderer(pRenderer);
		pRenderer = NULL; 
	}

	bool Load(ReloadDesc* pReloadDesc)
	{
		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			addShaders();
			addRootSignatures();
			addDescriptorSets();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			if (!addSwapChain())
				return false;

			if (!addDepthBuffer())
				return false;
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			addPipelines();
		}

		prepareDescriptorSets();

		UserInterfaceLoadDesc uiLoad = {};
		uiLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
		uiLoad.mHeight = mSettings.mHeight;
		uiLoad.mWidth = mSettings.mWidth;
		uiLoad.mLoadType = pReloadDesc->mType;
		loadUserInterface(&uiLoad);

		FontSystemLoadDesc fontLoad = {};
		fontLoad.mColorFormat = pSwapChain->ppRenderTargets[0]->mFormat;
		fontLoad.mHeight = mSettings.mHeight;
		fontLoad.mWidth = mSettings.mWidth;
		fontLoad.mLoadType = pReloadDesc->mType;
		loadFontSystem(&fontLoad);

		return true;
	}

	void Unload(ReloadDesc* pReloadDesc)
	{
		waitQueueIdle(pGraphicsQueue);

		unloadFontSystem(pReloadDesc->mType);
		unloadUserInterface(pReloadDesc->mType);

		if (pReloadDesc->mType & (RELOAD_TYPE_SHADER | RELOAD_TYPE_RENDERTARGET))
		{
			removePipelines();
		}

		if (pReloadDesc->mType & (RELOAD_TYPE_RESIZE | RELOAD_TYPE_RENDERTARGET))
		{
			removeSwapChain(pRenderer, pSwapChain);
			removeRenderTarget(pRenderer, pDepthBuffer);
		}

		if (pReloadDesc->mType & RELOAD_TYPE_SHADER)
		{
			removeDescriptorSets();
			removeRootSignatures();
			removeShaders();
		}
	}

	void Update(float deltaTime)
	{
		updateInputSystem(deltaTime, mSettings.mWidth, mSettings.mHeight);

		pCameraController->update(deltaTime);
		/************************************************************************/
		// Scene Update
		/************************************************************************/
		static float currentTime = 0.0f;
		currentTime += deltaTime * 1000.0f;

		// update camera with time
		mat4 viewMat = pCameraController->getViewMatrix();

		const float aspectInverse = (float)mSettings.mHeight / (float)mSettings.mWidth;
		const float horizontal_fov = PI / 2.0f;
		const vec3 scaleFactor = vec3(220.0f, 160.0f, 220.0f);

		CameraMatrix projMat = CameraMatrix::perspectiveReverseZ(horizontal_fov, aspectInverse, 0.1f, 10000.0f);
		gUniformData.mProjectView = projMat * viewMat;
		gUniformData.mToWorldMat = mat4::identity(); // mat4::scale(scaleFactor);
		// point light parameters
		gUniformData.mCameraPos = pCameraController->getViewPosition();

		//Spherical coordinate
		//float yaw = pViewParams.sunYawPitch.x;
		//float pitch = pViewParams.sunYawPitch.y;
		//Vector3 lightDir = Vector3(cos(yaw) * sin(pitch), cos(pitch), sin(yaw) * sin(pitch));
		//gUniformData.mSunDirection = -1.0f * lightDir;
		//
	}

	void Draw()
	{
		if (pSwapChain->mEnableVsync != mSettings.mVSyncEnabled)
		{
			waitQueueIdle(pGraphicsQueue);
			::toggleVSync(pRenderer, &pSwapChain);
		}

		uint32_t swapchainImageIndex;
		acquireNextImage(pRenderer, pSwapChain, pImageAcquiredSemaphore, NULL, &swapchainImageIndex);

		RenderTarget* pRenderTarget = pSwapChain->ppRenderTargets[swapchainImageIndex];
		Semaphore*    pRenderCompleteSemaphore = pRenderCompleteSemaphores[gFrameIndex];
		Fence*        pRenderCompleteFence = pRenderCompleteFences[gFrameIndex];

		// Stall if CPU is running "Swap Chain Buffer Count" frames ahead of GPU
		FenceStatus fenceStatus;
		getFenceStatus(pRenderer, pRenderCompleteFence, &fenceStatus);
		if (fenceStatus == FENCE_STATUS_INCOMPLETE)
			waitForFences(pRenderer, 1, &pRenderCompleteFence);

		if (pRenderer->pActiveGpuSettings->mGpuBreadcrumbs)
		{
			// Check breadcrumb markers
			checkMarkers();
		}

		// Update uniform buffers
		BufferUpdateDesc viewProjCbv = { pProjViewUniformBuffer[gFrameIndex] };
		beginUpdateResource(&viewProjCbv);
		*(UniformBlock*)viewProjCbv.pMappedData = gUniformData;
		endUpdateResource(&viewProjCbv, NULL);

		// Reset cmd pool for this frame
		resetCmdPool(pRenderer, pCmdPools[gFrameIndex]);

		Cmd* cmd = pCmds[gFrameIndex];
		beginCmd(cmd);

		if (pRenderer->pActiveGpuSettings->mGpuBreadcrumbs)
		{
			// Reset markers values
			resetMarkers(cmd);
		}

		cmdBeginGpuFrameProfile(cmd, gGpuProfileToken);

		RenderTargetBarrier barriers[] = {
			{ pRenderTarget, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET },
		};
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		// simply record the screen cleaning command
		LoadActionsDesc loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_CLEAR;
		loadActions.mLoadActionDepth = LOAD_ACTION_CLEAR;
		loadActions.mClearDepth.depth = 0.0f;
		loadActions.mClearColorValues[0] = { 0.3f, 0.6f, 0.68f, 1.0f };
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, pDepthBuffer, &loadActions, NULL, NULL, -1, -1);
		cmdSetViewport(cmd, 0.0f, 0.0f, (float)pRenderTarget->mWidth, (float)pRenderTarget->mHeight, 0.0f, 1.0f);
		cmdSetScissor(cmd, 0, 0, pRenderTarget->mWidth, pRenderTarget->mHeight);
		const uint32_t quadVbStride = sizeof(float) * 5;

		// ---------------------- Draw Quad
		cmdSetStencilReferenceValue(cmd, 0xFF);
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw Quad");
		Pipeline* quadPipeline = pQuadDrawPipeline;
		
		cmdBindPipeline(cmd, quadPipeline);
		cmdBindDescriptorSet(cmd, gFrameIndex * 2, pDescriptorSetUniforms);
		//cmdBindDescriptorSet(cmd, 0, pDescriptorSetCloudData);
		cmdBindVertexBuffer(cmd, 1, &pQuadVertexBuffer, &quadVbStride, NULL);
		
		cmdDraw(cmd, 6, 0);
		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		// ---------------------- Draw UI

		loadActions = {};
		loadActions.mLoadActionsColor[0] = LOAD_ACTION_LOAD;
		cmdBindRenderTargets(cmd, 1, &pRenderTarget, nullptr, &loadActions, NULL, NULL, -1, -1);
		cmdBeginGpuTimestampQuery(cmd, gGpuProfileToken, "Draw UI");

		gFrameTimeDraw.mFontColor = 0xff00ffff;
		gFrameTimeDraw.mFontSize = 18.0f;
		gFrameTimeDraw.mFontID = gFontID;
		float2 txtSizePx = cmdDrawCpuProfile(cmd, float2(8.f, 15.f), &gFrameTimeDraw);
		cmdDrawGpuProfile(cmd, float2(8.f, txtSizePx.y + 75.f), gGpuProfileToken, &gFrameTimeDraw);

		cmdDrawUserInterface(cmd);

		cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		cmdEndGpuTimestampQuery(cmd, gGpuProfileToken);

		barriers[0] = { pRenderTarget, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT };
		cmdResourceBarrier(cmd, 0, NULL, 0, NULL, 1, barriers);

		cmdEndGpuFrameProfile(cmd, gGpuProfileToken);
		endCmd(cmd);

		QueueSubmitDesc submitDesc = {};
		submitDesc.mCmdCount = 1;
		submitDesc.mSignalSemaphoreCount = 1;
		submitDesc.mWaitSemaphoreCount = 1;
		submitDesc.ppCmds = &cmd;
		submitDesc.ppSignalSemaphores = &pRenderCompleteSemaphore;
		submitDesc.ppWaitSemaphores = &pImageAcquiredSemaphore;
		submitDesc.pSignalFence = pRenderCompleteFence;
		queueSubmit(pGraphicsQueue, &submitDesc);
		QueuePresentDesc presentDesc = {};
		presentDesc.mIndex = swapchainImageIndex;
		presentDesc.mWaitSemaphoreCount = 1;
		presentDesc.pSwapChain = pSwapChain;
		presentDesc.ppWaitSemaphores = &pRenderCompleteSemaphore;
		presentDesc.mSubmitDone = true;

		// captureScreenshot() must be used before presentation.
		if (gTakeScreenshot)
		{
			// Metal platforms need one renderpass to prepare the swapchain textures for copy.
			if(prepareScreenshot(pSwapChain))
			{
				captureScreenshot(pSwapChain, swapchainImageIndex, RESOURCE_STATE_PRESENT, "02_CloudRendering.png");
				gTakeScreenshot = false;
			}
		}
		
		queuePresent(pGraphicsQueue, &presentDesc);
		flipProfiler();

		gFrameIndex = (gFrameIndex + 1) % gImageCount;
	}

	const char* GetName() { return "02_CloudRendering"; }

	bool addSwapChain()
	{
		SwapChainDesc swapChainDesc = {};
		swapChainDesc.mWindowHandle = pWindow->handle;
		swapChainDesc.mPresentQueueCount = 1;
		swapChainDesc.ppPresentQueues = &pGraphicsQueue;
		swapChainDesc.mWidth = mSettings.mWidth;
		swapChainDesc.mHeight = mSettings.mHeight;
		swapChainDesc.mImageCount = gImageCount;
		swapChainDesc.mColorFormat = getRecommendedSwapchainFormat(true, true);
		swapChainDesc.mEnableVsync = mSettings.mVSyncEnabled;
        swapChainDesc.mFlags = SWAP_CHAIN_CREATION_FLAG_ENABLE_FOVEATED_RENDERING_VR;
		::addSwapChain(pRenderer, &swapChainDesc, &pSwapChain);

		return pSwapChain != NULL;
	}

	bool addDepthBuffer()
	{
		// Add depth buffer
		RenderTargetDesc depthRT = {};
		depthRT.mArraySize = 1;
		depthRT.mClearValue.depth = 0.0f;
		depthRT.mClearValue.stencil = 0;
		depthRT.mDepth = 1;
		depthRT.mFormat = TinyImageFormat_D32_SFLOAT;
		depthRT.mStartState = RESOURCE_STATE_DEPTH_WRITE;
		depthRT.mHeight = mSettings.mHeight;
		depthRT.mSampleCount = SAMPLE_COUNT_1;
		depthRT.mSampleQuality = 0;
		depthRT.mWidth = mSettings.mWidth;
		depthRT.mFlags = TEXTURE_CREATION_FLAG_ON_TILE | TEXTURE_CREATION_FLAG_VR_MULTIVIEW;
		addRenderTarget(pRenderer, &depthRT, &pDepthBuffer);

		return pDepthBuffer != NULL;
	}

	void addDescriptorSets()
	{
		//DescriptorSetDesc desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		//addDescriptorSet(pRenderer, &desc, &pDescriptorSetTexture);
		DescriptorSetDesc desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, gImageCount * 2 };
		addDescriptorSet(pRenderer, &desc, &pDescriptorSetUniforms);
		//desc = { pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
		//addDescriptorSet(pRenderer, &desc, &pDescriptorSetCloudData);
	}

	void removeDescriptorSets()
	{
		//removeDescriptorSet(pRenderer, pDescriptorSetTexture);
		removeDescriptorSet(pRenderer, pDescriptorSetUniforms);
		//removeDescriptorSet(pRenderer, pDescriptorSetCloudData);
	}

	void addRootSignatures()
	{
		Shader* shaders[1];
		uint32_t shadersCount = 1;
		shaders[0] = pQuadDrawShader;

		const char* pStaticSamplerNames[] = { "uSampler0", "uSamplerCloud"};
		Sampler* pStaticSamplers[] = { pSamplerQuad, pSamplerCloud };
		RootSignatureDesc rootDesc = {};
		rootDesc.mStaticSamplerCount = 2;
		rootDesc.ppStaticSamplerNames = pStaticSamplerNames;
		rootDesc.ppStaticSamplers = pStaticSamplers;
		rootDesc.mShaderCount = shadersCount;
		rootDesc.ppShaders = shaders;
		addRootSignature(pRenderer, &rootDesc, &pRootSignature);
	}

	void removeRootSignatures()
	{
		removeRootSignature(pRenderer, pRootSignature);
	}

	void addShaders()
	{
		ShaderLoadDesc skyShader = {};
		skyShader.mStages[0] = { "skybox.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		skyShader.mStages[1] = { "skybox.frag", NULL, 0 };
		ShaderLoadDesc quadShader = {};
		quadShader.mStages[0] = { "quad.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		quadShader.mStages[1] = { "quad.frag", NULL, 0 };
		ShaderLoadDesc cubeShader = {};
		cubeShader.mStages[0] = { "cube.vert", NULL, 0, NULL, SHADER_STAGE_LOAD_FLAG_ENABLE_VR_MULTIVIEW };
		cubeShader.mStages[1] = { "cube.frag", NULL, 0 };

		addShader(pRenderer, &quadShader, &pQuadDrawShader);
	}

	void removeShaders()
	{
		removeShader(pRenderer, pQuadDrawShader);
	}

	void addPipelines()
	{
		RasterizerStateDesc rasterizerStateDesc = {};
		rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		DepthStateDesc depthStateDesc = {};
		depthStateDesc.mDepthTest = true;
		depthStateDesc.mDepthWrite = true;
		depthStateDesc.mDepthFunc = CMP_GEQUAL;

		// ------------------------------------ layout and pipeline for quad draw
		VertexLayout quadVertexLayout = {};
		quadVertexLayout.mAttribCount = 2;
		quadVertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
		quadVertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
		quadVertexLayout.mAttribs[0].mBinding = 0;
		quadVertexLayout.mAttribs[0].mLocation = 0;
		quadVertexLayout.mAttribs[0].mOffset = 0;
		quadVertexLayout.mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
		quadVertexLayout.mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
		quadVertexLayout.mAttribs[1].mBinding = 0;
		quadVertexLayout.mAttribs[1].mLocation = 1;
		quadVertexLayout.mAttribs[1].mOffset = 3 * sizeof(float);

		RasterizerStateDesc quadRasterizerStateDesc = {};
		//quadRasterizerStateDesc.mCullMode = CULL_MODE_BACK;
		quadRasterizerStateDesc.mCullMode = CULL_MODE_NONE;

		DepthStateDesc quadDepthStateDesc = {};
		quadDepthStateDesc.mDepthTest = true;
		quadDepthStateDesc.mDepthWrite = true;
		quadDepthStateDesc.mDepthFunc = CMP_GEQUAL;

		PipelineDesc quadDesc = {};
		quadDesc.mType = PIPELINE_TYPE_GRAPHICS;
		GraphicsPipelineDesc& quadPipelineSettings = quadDesc.mGraphicsDesc;
		quadPipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
		quadPipelineSettings.mRenderTargetCount = 1;
		quadPipelineSettings.pDepthState = &quadDepthStateDesc;
		quadPipelineSettings.pColorFormats = &pSwapChain->ppRenderTargets[0]->mFormat;
		quadPipelineSettings.mSampleCount = pSwapChain->ppRenderTargets[0]->mSampleCount;
		quadPipelineSettings.mSampleQuality = pSwapChain->ppRenderTargets[0]->mSampleQuality;
		quadPipelineSettings.mDepthStencilFormat = pDepthBuffer->mFormat;
		quadPipelineSettings.pRootSignature = pRootSignature;
		quadPipelineSettings.pShaderProgram = pQuadDrawShader;
		quadPipelineSettings.pVertexLayout = &quadVertexLayout;
		quadPipelineSettings.pRasterizerState = &quadRasterizerStateDesc;
		quadPipelineSettings.mVRFoveatedRendering = true;
		addPipeline(pRenderer, &quadDesc, &pQuadDrawPipeline);
	}

	void removePipelines()
	{
		removePipeline(pRenderer, pQuadDrawPipeline);
	}

	void prepareDescriptorSets()
	{
		/*DescriptorData cloudParams[3] = {};
		cloudParams[0].pName = "WeatherTexture";
		cloudParams[0].ppTextures = &pWeatherTexture;
		cloudParams[1].pName = "BlueNoiseTexture";
		cloudParams[1].ppTextures = &pBlueNoiseTexture;
		cloudParams[2].pName = "CloudShape";
		cloudParams[2].ppTextures = &pCloudShapeTexture;
		updateDescriptorSet(pRenderer, 0, pDescriptorSetCloudData, 3, cloudParams);*/

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			DescriptorData params[1] = {};
			params[0].pName = "uniformBlock";
			params[0].ppBuffers = &pProjViewUniformBuffer[i];
			updateDescriptorSet(pRenderer, i * 2, pDescriptorSetUniforms, 1, params);
		}
	}

	void initMarkers()
	{
		BufferLoadDesc breadcrumbBuffer = {};
		breadcrumbBuffer.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNDEFINED;
		breadcrumbBuffer.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
		breadcrumbBuffer.mDesc.mSize = (gMarkerCount + 3) / 4 * 4 * sizeof(uint32_t);
		breadcrumbBuffer.mDesc.mFlags = BUFFER_CREATION_FLAG_NONE;
		breadcrumbBuffer.mDesc.mStartState = RESOURCE_STATE_COPY_DEST;
		breadcrumbBuffer.pData = NULL;

		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			breadcrumbBuffer.ppBuffer = &pMarkerBuffer[i];
			addResource(&breadcrumbBuffer, NULL);
		}
	}

	void checkMarkers()
	{
		if (bHasCrashed)
		{
			ReadRange readRange = { 0, gMarkerCount * sizeof(uint32_t) };
			mapBuffer(pRenderer, pMarkerBuffer[gCrashedFrame], &readRange);

			uint32_t* markersValue = (uint32_t*)pMarkerBuffer[gCrashedFrame]->pCpuMappedAddress;

			for (uint32_t m = 0; m < gMarkerCount; ++m)
			{
				if (gValidMarkerValue != markersValue[m])
				{
					LOGF(LogLevel::eERROR, "[Breadcrumb] crashed frame: %u, marker: %u, value:%u", gCrashedFrame, m, markersValue[m]);
				}
			}

			unmapBuffer(pRenderer, pMarkerBuffer[gCrashedFrame]);

			bHasCrashed = false;
		}
	}

	void resetMarkers(Cmd* pCmd)
	{
		for (uint32_t i = 0; i < gMarkerCount; ++i)
		{
			cmdWriteMarker(pCmd, MARKER_TYPE_DEFAULT, 0, pMarkerBuffer[gFrameIndex], i, false);
		}
	}

	void exitMarkers()
	{
		for (uint32_t i = 0; i < gImageCount; ++i)
		{
			removeResource(pMarkerBuffer[i]);
		}
	}
};

DEFINE_APPLICATION_MAIN(Transformations)
