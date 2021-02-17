#include "pch.h"
#include "RenderFrontend.h"

//disable ImGui warning
#pragma warning( push )
#pragma warning( disable : 26495)

#include <imgui/imgui.h>

//reenable warning
#pragma warning( pop )

#include <Utilities/MathUtils.h>
#include "Runtime/Timer.h"
#include "Culling.h"
#include "Utilities/GeneralUtils.h"
#include "Common/MeshProcessing.h"
#include "Noise.h"
#include "Common/Utilities/DirectoryUtils.h"
#include "Common/ImageIO.h"
#include "Common/VolumeInfo.h"
#include "Common/sdfUtilities.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

//definition of extern variable from header
RenderFrontend gRenderFrontend;

const uint32_t shadowMapRes = 2048;
const uint32_t skyTextureRes = 1024;
const uint32_t specularSkyProbeRes = 128;
const uint32_t diffuseSkyProbeRes = 4;
const uint32_t skyTextureMipCount = 8;
const uint32_t brdfLutRes = 512;
const uint32_t nHistogramBins = 128;
const uint32_t shadowCascadeCount = 4;
const uint32_t skyTransmissionLutResolution = 128;
const uint32_t skyMultiscatterLutResolution = 32;
const uint32_t skyLutWidth = 200;
const uint32_t skyLutHeight = 100;

const uint32_t histogramTileSizeX = 32;
const uint32_t histogramTileSizeY = 32;

const uint32_t  skyShadowMapRes = 1024;
const uint32_t  skyOcclusionVolumeMaxRes = 256;
const float     skyOcclusionTargetDensity = 0.5f; //meter/texel
const uint32_t  skyOcclusionSampleCount = 1024;

const uint32_t noiseTextureCount = 4;
const uint32_t noiseTextureWidth = 32;
const uint32_t noiseTextureHeight = 32;

const uint32_t shadowSampleCount = 8;

const uint32_t maxObjectCountMainScene = 300;

const size_t sdfCameraCullingTileSize = 32;
const size_t maxSdfObjectsPerTile = 100;

const uint32_t sdfShadowCullingTilesCount1D = 32;

//bindings of global shader uniforms
const uint32_t globalUniformBufferBinding               = 0;
const uint32_t globalSamplerAnisotropicRepeatBinding    = 1;
const uint32_t globalSamplerNearestBlackBorderBinding   = 2;
const uint32_t globalSamplerLinearRepeatBinding         = 3;
const uint32_t globalSamplerLinearClampBinding          = 4;
const uint32_t globalSamplerNearestClampBinding         = 5;
const uint32_t globalSamplerLinearWhiteBorderBinding    = 6;
const uint32_t globalSamplerNearestRepeatBinding		= 7;
const uint32_t globalNoiseTextureBindingBinding         = 8;

//some SPD shaders avoids annyoing extra logic for low mips, which are never used anyways
const uint32_t indirectLightingMaxMipCount = 7;	
const uint32_t edgeMaxMipCount = 7;

const glm::uvec3 sceneMaterialVoxelTextureResolution = glm::uvec3(64);

//currently rendering without attachment is not supported, so a small dummy is used
const ImageFormat dummyImageFormat = ImageFormat::R8;

//matrices used in the main rendering pass for depth-prepass and forward
struct MainPassMatrices {
	glm::mat4 model;
	glm::mat4 mvp;
	glm::mat4 mvpPrevious;
};

struct ShadowFrustumInfo {
	glm::vec3 frustumUp;		float padding0;
	glm::vec3 frustumRight;		float padding1;
	glm::vec2 frustumExtends;
	glm::vec2 frustumOffset;
};

void resizeCallback(GLFWwindow* window, int width, int height) {
    RenderFrontend* frontEnd = reinterpret_cast<RenderFrontend*>(glfwGetWindowUserPointer(window));
    frontEnd->setResolution(width, height);
}

DefaultTextures createDefaultTextures() {
    DefaultTextures defaultTextures;
    //albedo
    {
        ImageDescription defaultDiffuseDesc;
        defaultDiffuseDesc.autoCreateMips = true;
        defaultDiffuseDesc.depth = 1;
        defaultDiffuseDesc.format = ImageFormat::RGBA8;
        defaultDiffuseDesc.initialData = { 255, 255, 255, 255 };
        defaultDiffuseDesc.manualMipCount = 1;
        defaultDiffuseDesc.mipCount = MipCount::FullChain;
        defaultDiffuseDesc.type = ImageType::Type2D;
        defaultDiffuseDesc.usageFlags = ImageUsageFlags::Sampled;
        defaultDiffuseDesc.width = 1;
        defaultDiffuseDesc.height = 1;

        defaultTextures.diffuse = gRenderBackend.createImage(defaultDiffuseDesc);
    }
    //specular
    {
        ImageDescription defaultSpecularDesc;
        defaultSpecularDesc.autoCreateMips = true;
        defaultSpecularDesc.depth = 1;
        defaultSpecularDesc.format = ImageFormat::RGBA8;
        defaultSpecularDesc.initialData = { 0, 128, 255, 0 };
        defaultSpecularDesc.manualMipCount = 1;
        defaultSpecularDesc.mipCount = MipCount::FullChain;
        defaultSpecularDesc.type = ImageType::Type2D;
        defaultSpecularDesc.usageFlags = ImageUsageFlags::Sampled;
        defaultSpecularDesc.width = 1;
        defaultSpecularDesc.height = 1;

        defaultTextures.specular = gRenderBackend.createImage(defaultSpecularDesc);
    }
    //normal
    {
        ImageDescription defaultNormalDesc;
        defaultNormalDesc.autoCreateMips = true;
        defaultNormalDesc.depth = 1;
        defaultNormalDesc.format = ImageFormat::RG8;
        defaultNormalDesc.initialData = { 128, 128 };
        defaultNormalDesc.manualMipCount = 1;
        defaultNormalDesc.mipCount = MipCount::FullChain;
        defaultNormalDesc.type = ImageType::Type2D;
        defaultNormalDesc.usageFlags = ImageUsageFlags::Sampled;
        defaultNormalDesc.width = 1;
        defaultNormalDesc.height = 1;

        defaultTextures.normal = gRenderBackend.createImage(defaultNormalDesc);
    }
    //sky
    {
        ImageDescription defaultCubemapDesc;
        defaultCubemapDesc.autoCreateMips = true;
        defaultCubemapDesc.depth = 1;
        defaultCubemapDesc.format = ImageFormat::RGBA8;
        defaultCubemapDesc.initialData = { 255, 255, 255, 255 };
        defaultCubemapDesc.manualMipCount = 1;
        defaultCubemapDesc.mipCount = MipCount::FullChain;
        defaultCubemapDesc.type = ImageType::Type2D;
        defaultCubemapDesc.usageFlags = ImageUsageFlags::Sampled;
        defaultCubemapDesc.width = 1;
        defaultCubemapDesc.height = 1;

        defaultTextures.sky = gRenderBackend.createImage(defaultCubemapDesc);
    }
	//sdf
	{
		ImageDescription defaultSdfDesc;
		defaultSdfDesc.autoCreateMips = false;
		defaultSdfDesc.depth = 1;
		defaultSdfDesc.format = ImageFormat::R16_sFloat;
		defaultSdfDesc.initialData = { 0, 0 };
		defaultSdfDesc.manualMipCount = 1;
		defaultSdfDesc.mipCount = MipCount::One;
		defaultSdfDesc.type = ImageType::Type3D;
		defaultSdfDesc.usageFlags = ImageUsageFlags::Sampled;
		defaultSdfDesc.width = 1;
		defaultSdfDesc.height = 1;
		defaultSdfDesc.depth = 1;

		defaultTextures.sdf = gRenderBackend.createImage(defaultSdfDesc);
	}
    return defaultTextures;
}

//returns jitter in pixels, must be multiplied with texel size before applying to projection matrix
glm::vec2 computeProjectionMatrixJitter() {

    glm::vec2 offset;
    static int jitterIndex;
    jitterIndex++;
    jitterIndex %= 8;
    offset = 2.f * hammersley2D(jitterIndex) - 1.f;

    return offset;
}

glm::mat4 applyProjectionMatrixJitter(const glm::mat4& projectionMatrix, const glm::vec2& offset) {

    glm::mat4 jitteredProjection = projectionMatrix;
    jitteredProjection[2][0] = offset.x;
    jitteredProjection[2][1] = offset.y;

    return jitteredProjection;
}

//jitter must be in pixels, so before multipliying with screen pixel size
std::array<float, 9> computeTaaResolveWeights(const glm::vec2 cameraJitterInPixels) {
    std::array<float, 9> weights = {};
    int index = 0;
    float totalWeight = 0.f;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            const float d = glm::length(cameraJitterInPixels - glm::vec2(x, y));
            //gaussian fit to blackman-Harris 3.3
            //reference: "High Quality Temporal Supersampling", page 23
            const float w = glm::exp(-2.29f * d*d);
            weights[index] = w;
            totalWeight += w;
            index++;
        }
    }

    for (float& w : weights) {
        w /= totalWeight;
    }
    return weights;
}

void RenderFrontend::setup(GLFWwindow* window) {
    m_window = window;

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    m_screenWidth = width;
    m_screenHeight = height;
    m_camera.intrinsic.aspectRatio = (float)width / (float)height;

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, resizeCallback);

    setupGlobalShaderInfoLayout();

    m_defaultTextures = createDefaultTextures();
    initSamplers();
    initImages();

    const auto histogramSettings = createHistogramSettings();
    initBuffers(histogramSettings);
    initRenderpasses(histogramSettings);
    initFramebuffers();
    initRenderTargets();
    initMeshs();

	setupGlobalShaderInfoResources();

    gRenderBackend.newFrame();
    computeBRDFLut();
    gRenderBackend.renderFrame(false);
}

void RenderFrontend::shutdown() {
    
}

void RenderFrontend::prepareNewFrame() {
    if (m_didResolutionChange) {

        gRenderBackend.recreateSwapchain(m_screenWidth, m_screenHeight, m_window);
        gRenderBackend.resizeImages({
            m_frameRenderTargets[0].motionBuffer,
			m_frameRenderTargets[1].motionBuffer,
            m_frameRenderTargets[0].colorBuffer,
            m_frameRenderTargets[0].depthBuffer,
            m_frameRenderTargets[1].colorBuffer,
            m_frameRenderTargets[1].depthBuffer,
            m_postProcessBuffers[0],
            m_postProcessBuffers[1],
            m_historyBuffers[0],
            m_historyBuffers[1],
            m_sceneLuminance,
            m_lastFrameLuminance,
			m_worldSpaceNormalImage,
			m_indirectLightingFullRes_Y_SH, 
			m_indirectLightingFullRes_CoCg },
            m_screenWidth, m_screenHeight);
        gRenderBackend.resizeImages({ m_minMaxDepthPyramid, m_depthHalfRes }, m_screenWidth / 2, m_screenHeight / 2);

		resizeIndirectLightingBuffers();

        m_didResolutionChange = false;

        uint32_t threadgroupCount = 0;
        gRenderBackend.updateComputePassShaderDescription(m_depthPyramidPass, createDepthPyramidShaderDescription(&threadgroupCount));

        m_globalShaderInfo.cameraCut = true;
    }
    if (m_minimized) {
        return;
    }

    if (m_isMainPassShaderDescriptionStale) {
        gRenderBackend.updateGraphicPassShaderDescription(m_mainPass, createForwardPassShaderDescription(m_shadingConfig));
        m_isMainPassShaderDescriptionStale = false;
    }

    if (m_isBRDFLutShaderDescriptionStale) {
        gRenderBackend.updateComputePassShaderDescription(m_brdfLutPass, createBRDFLutShaderDescription(m_shadingConfig));
        //don't reset m_isMainPassShaderDescriptionStale, this is done when rendering as it's used to trigger lut recreation
    }

    if (m_isTemporalFilterShaderDescriptionStale) {
        gRenderBackend.updateComputePassShaderDescription(m_temporalFilterPass, createTemporalFilterShaderDescription());
        m_isTemporalFilterShaderDescriptionStale = false;
    }
    if (m_isTemporalSupersamplingShaderDescriptionStale) {
        gRenderBackend.updateComputePassShaderDescription(m_temporalSupersamplingPass, createTemporalSupersamplingShaderDescription());
        m_isTemporalSupersamplingShaderDescriptionStale = false;
    }

    gRenderBackend.updateShaderCode();
    gRenderBackend.newFrame();    

    const auto tempImage = m_sceneLuminance;
    m_sceneLuminance = m_lastFrameLuminance;
    m_lastFrameLuminance = tempImage;

    drawUi();
    prepareRenderpasses();
    gRenderBackend.startDrawcallRecording();

	m_currentMeshCount = 0;
	m_currentMainPassDrawcallCount = 0;
	m_currentShadowPassDrawcallCount = 0;
}

void RenderFrontend::setupGlobalShaderInfoLayout() {
    ShaderLayout globalLayout;
    globalLayout.uniformBufferBindings.push_back(globalUniformBufferBinding);
    globalLayout.sampledImageBindings.push_back(globalNoiseTextureBindingBinding);

    globalLayout.samplerBindings.push_back(globalSamplerAnisotropicRepeatBinding);
    globalLayout.samplerBindings.push_back(globalSamplerNearestBlackBorderBinding);
    globalLayout.samplerBindings.push_back(globalSamplerLinearRepeatBinding);
    globalLayout.samplerBindings.push_back(globalSamplerLinearClampBinding);
    globalLayout.samplerBindings.push_back(globalSamplerNearestClampBinding);
    globalLayout.samplerBindings.push_back(globalSamplerLinearWhiteBorderBinding);
	globalLayout.samplerBindings.push_back(globalSamplerNearestRepeatBinding);
    
    gRenderBackend.setGlobalDescriptorSetLayout(globalLayout);
}

void RenderFrontend::setupGlobalShaderInfoResources() {
	RenderPassResources globalResources;
	globalResources.uniformBuffers = { UniformBufferResource(m_globalUniformBuffer, globalUniformBufferBinding) };
	globalResources.samplers = {
		SamplerResource(m_sampler_anisotropicRepeat,    globalSamplerAnisotropicRepeatBinding),
		SamplerResource(m_sampler_nearestBlackBorder,   globalSamplerNearestBlackBorderBinding),
		SamplerResource(m_sampler_linearRepeat,         globalSamplerLinearRepeatBinding),
		SamplerResource(m_sampler_linearClamp,          globalSamplerLinearClampBinding),
		SamplerResource(m_sampler_nearestClamp,         globalSamplerNearestClampBinding),
		SamplerResource(m_sampler_linearWhiteBorder,    globalSamplerLinearWhiteBorderBinding),
		SamplerResource(m_sampler_nearestRepeat,		globalSamplerNearestRepeatBinding),
	};
	globalResources.sampledImages = { ImageResource(m_noiseTextures[m_noiseTextureIndex], 0, globalNoiseTextureBindingBinding) };
	gRenderBackend.setGlobalDescriptorSetResources(globalResources);
}

void RenderFrontend::prepareRenderpasses(){

    std::vector<RenderPassHandle> forwardShadingDependencies;

    if (m_isBRDFLutShaderDescriptionStale) {
        computeBRDFLut();
        forwardShadingDependencies.push_back(m_brdfLutPass);
        m_isBRDFLutShaderDescriptionStale = false;
    }

	if (m_shadingConfig.indirectLightingTech == IndirectLightingTech::SDFTrace) {
		forwardShadingDependencies.push_back(m_diffuseSDFTracePass);
		forwardShadingDependencies.push_back(m_indirectDiffuseFilterSpatialPass[0]);
		forwardShadingDependencies.push_back(m_indirectDiffuseFilterSpatialPass[1]);
		forwardShadingDependencies.push_back(m_indirectDiffuseFilterTemporalPass);
		if (m_shadingConfig.indirectLightingHalfRes) {
			forwardShadingDependencies.push_back(m_indirectLightingUpscale);
		}
	}

    static int sceneRenderTargetIndex;
    const FrameRenderTargets lastRenderTarget = m_frameRenderTargets[sceneRenderTargetIndex];
    sceneRenderTargetIndex++;
    sceneRenderTargetIndex %= 2;
    const FrameRenderTargets currentRenderTarget = m_frameRenderTargets[sceneRenderTargetIndex];

    renderSunShadowCascades();
    computeColorBufferHistogram(lastRenderTarget.colorBuffer);
    computeExposure();
    renderDepthPrepass(currentRenderTarget.prepassFramebuffer);
    computeDepthPyramid(currentRenderTarget.depthBuffer);
    computeSunLightMatrices();
	if (m_shadingConfig.indirectLightingTech == IndirectLightingTech::SDFTrace) {
		if (m_shadingConfig.indirectLightingHalfRes) {
			downscaleDepth(currentRenderTarget);
		}		
		diffuseSDFTrace(currentRenderTarget);
		filterIndirectDiffuse(currentRenderTarget, lastRenderTarget);
	}
    renderForwardShading(forwardShadingDependencies, currentRenderTarget.colorFramebuffer);
	RenderPassHandle skyParent = m_mainPass;
	if (m_renderBoundingBoxes) {
		renderDebugGeometry(currentRenderTarget.colorFramebuffer);
		skyParent = m_debugGeoPass;
	}
    renderSky(currentRenderTarget.colorFramebuffer, skyParent);
    
    RenderPassHandle currentPass = m_sunSpritePass;
    ImageHandle currentSrc = currentRenderTarget.colorBuffer;

    if (m_temporalFilterSettings.enabled) {

        if (m_temporalFilterSettings.useSeparateSupersampling) {
            computeTemporalSuperSampling(currentRenderTarget, lastRenderTarget,
                m_postProcessBuffers[0], currentPass);

            currentPass = m_temporalSupersamplingPass;
            currentSrc = m_postProcessBuffers[0];
        }

        static int historyIndex;
        const ImageHandle lastHistory = m_historyBuffers[historyIndex];
        historyIndex++;
        historyIndex %= 2;
        const ImageHandle currentHistory = m_historyBuffers[historyIndex];

        computeTemporalFilter(currentSrc, currentRenderTarget, m_postProcessBuffers[1], currentPass, currentHistory, lastHistory);
        currentPass = m_temporalFilterPass;
        currentSrc = m_postProcessBuffers[1];        
    }

    if (m_sdfDebugMode == SDFDebugMode::VisualizeSDF) {
		renderSDFDebug({ currentPass });
		currentPass = m_sdfDebugPass;
		currentSrc = m_postProcessBuffers[0];
    }

    computeTonemapping(currentPass, currentSrc);
}

void RenderFrontend::setResolution(const uint32_t width, const uint32_t height) {
    m_screenWidth = width;
    m_screenHeight = height;
    //avoid zeros when minimzed
    m_camera.intrinsic.aspectRatio = glm::max((float)width / glm::max((float)height, 0.001f), 0.001f);
    if (width == 0 || height == 0) {
        m_minimized = true;
        return;
    }
    else {
        m_minimized = false;
    }
    m_didResolutionChange = true;
}

void RenderFrontend::setCameraExtrinsic(const CameraExtrinsic& extrinsic) {

    m_previousViewProjectionMatrix = m_viewProjectionMatrix;
    m_globalShaderInfo.previousFrameCameraJitter = m_globalShaderInfo.currentFrameCameraJitter;

    m_camera.extrinsic = extrinsic;
    const glm::mat4 viewMatrix = viewMatrixFromCameraExtrinsic(extrinsic);
    const glm::mat4 projectionMatrix = projectionMatrixFromCameraIntrinsic(m_camera.intrinsic);

    //jitter matrix for temporal supersampling
    if(m_temporalFilterSettings.enabled){
        const glm::vec2 pixelSize = glm::vec2(1.f / m_screenWidth, 1.f / m_screenHeight);

        const glm::vec2 jitterInPixels = computeProjectionMatrixJitter();
        const std::array<float, 9> resolveWeights = computeTaaResolveWeights(jitterInPixels);
        gRenderBackend.setUniformBufferData(m_taaResolveWeightBuffer, &resolveWeights[0], sizeof(float)*9);

        m_globalShaderInfo.currentFrameCameraJitter = jitterInPixels * pixelSize;
        const glm::mat4 jitteredProjection = applyProjectionMatrixJitter(projectionMatrix, m_globalShaderInfo.currentFrameCameraJitter);

        m_viewProjectionMatrix = jitteredProjection * viewMatrix;
    }    
    else {
        m_globalShaderInfo.currentFrameCameraJitter = glm::vec2(0.f);
        m_viewProjectionMatrix = projectionMatrix * viewMatrix;
    }

	m_globalShaderInfo.viewProjection = m_viewProjectionMatrix;

    updateCameraFrustum();
    updateShadowFrustum();
}

std::vector<MeshHandleFrontend> RenderFrontend::registerMeshes(const std::vector<MeshBinary>& meshes){
    
    std::vector<Material> materials;
    materials.reserve(meshes.size());

	const std::vector<MeshHandle> backendHandles = gRenderBackend.createMeshes(meshes);

	std::vector<MeshHandleFrontend> meshHandlesFrontend;
	meshHandlesFrontend.reserve(backendHandles.size());
	
	assert(backendHandles.size() == meshes.size());
	for (int i = 0; i < backendHandles.size(); i++) {
		MeshHandleFrontend meshHandleFrontend;
		meshHandleFrontend.index = m_frontendMeshes.size();
		meshHandlesFrontend.push_back(meshHandleFrontend);

		MeshFrontend meshFrontend;
		meshFrontend.backendHandle = backendHandles[i];

		const MeshBinary mesh = meshes[i];

		meshFrontend.localBB = mesh.boundingBox;
		meshFrontend.meanAlbedo = mesh.meanAlbedo;

		//material
		ImageHandle albedoHandle;
		if (!loadImageFromPath(mesh.texturePaths.albedoTexturePath, &albedoHandle)) {
			albedoHandle = m_defaultTextures.diffuse;
		}
		ImageHandle normalHandle;
		if (!loadImageFromPath(mesh.texturePaths.normalTexturePath, &normalHandle)) {
			normalHandle = m_defaultTextures.normal;
		}
		ImageHandle specularHandle;
		if (!loadImageFromPath(mesh.texturePaths.specularTexturePath, &specularHandle)) {
			specularHandle = m_defaultTextures.specular;
		}
		ImageHandle sdfHandle;
		if (!loadImageFromPath(mesh.texturePaths.sdfTexturePath, &sdfHandle)) {
			sdfHandle = m_defaultTextures.sdf;
		}

		meshFrontend.material.albedoTextureIndex = gRenderBackend.getImageGlobalTextureArrayIndex(albedoHandle);
		meshFrontend.material.normalTextureIndex = gRenderBackend.getImageGlobalTextureArrayIndex(normalHandle);
		meshFrontend.material.specularTextureIndex = gRenderBackend.getImageGlobalTextureArrayIndex(specularHandle);
		meshFrontend.sdfTextureIndex = gRenderBackend.getImageGlobalTextureArrayIndex(sdfHandle);

		m_frontendMeshes.push_back(meshFrontend);
	}
	return meshHandlesFrontend;
}

void RenderFrontend::prepareForDrawcalls() {
    if (m_minimized) {
        return;
    }
    //global shader info must be updated before drawcalls, else they would be invalidated by descriptor set update
    //cant update at frame start as camera data must be set before updaing global info
    updateGlobalShaderInfo();
}

void RenderFrontend::renderScene(const std::vector<RenderObject>& scene) {

    //if we prepare render commands without consuming them we will save up a huge amount of commands
    //to avoid this commands are not recorded if minmized
    if (m_minimized) {
        return;
    }

    m_currentMeshCount += (uint32_t)scene.size();

    //main and prepass
    {
		struct MainPassPushConstants {
			uint32_t albedoTextureIndex;
			uint32_t normalTextureIndex;
			uint32_t specularTextureIndex;
			uint32_t transformIndex;
		};

        std::vector<MeshHandle> culledMeshes;
		std::vector<MainPassPushConstants> pushConstants;
		std::vector<MainPassMatrices> mainPassMatrices;

        //frustum culling
        for (const RenderObject& obj : scene) {

            const bool isVisible = isAxisAlignedBoundingBoxIntersectingViewFrustum(m_cameraFrustum, obj.bbWorld);

            if (isVisible) {
                m_currentMainPassDrawcallCount++;
				MeshFrontend meshFrontend = m_frontendMeshes[obj.mesh.index];
                culledMeshes.push_back(meshFrontend.backendHandle);

				MainPassPushConstants meshPushConstants;
				meshPushConstants.albedoTextureIndex = meshFrontend.material.albedoTextureIndex;
				meshPushConstants.normalTextureIndex = meshFrontend.material.normalTextureIndex;
				meshPushConstants.specularTextureIndex = meshFrontend.material.specularTextureIndex;
				meshPushConstants.transformIndex = mainPassMatrices.size();
				pushConstants.push_back(meshPushConstants);

				MainPassMatrices matrices;
				matrices.model = obj.modelMatrix;
				matrices.mvp = m_viewProjectionMatrix * obj.modelMatrix;
				matrices.mvpPrevious = m_previousViewProjectionMatrix * obj.previousModelMatrix;
				mainPassMatrices.push_back(matrices);
            }
        }
        gRenderBackend.drawMeshes(culledMeshes, (char*)pushConstants.data(), m_mainPass);
        gRenderBackend.drawMeshes(culledMeshes, (char*)pushConstants.data(), m_depthPrePass);

		gRenderBackend.setStorageBufferData(m_mainPassTransformsBuffer, mainPassMatrices.data(), 
			sizeof(MainPassMatrices) * mainPassMatrices.size());
    }
    
    //shadow pass
    {
		struct ShadowPushConstants {
			uint32_t albedoTextureIndex;
			uint32_t transformIndex;
		};

        std::vector<MeshHandle> culledMeshes;
		std::vector<glm::mat4> modelMatrices;
		std::vector<ShadowPushConstants> pushConstantData;

        const glm::vec3 sunDirection = directionToVector(m_sunDirection);
        //we must not cull behind the shadow frustum near plane, as objects there cast shadows into the visible area
        //for now we simply offset the near plane points very far into the light direction
        //this means that all objects in that direction within the moved distance will intersect our frustum and aren't culled
        const float nearPlaneExtensionLength = 10000.f;
        const glm::vec3 nearPlaneOffset = sunDirection * nearPlaneExtensionLength;
        m_sunShadowFrustum.points.l_l_n += nearPlaneOffset;
        m_sunShadowFrustum.points.r_l_n += nearPlaneOffset;
        m_sunShadowFrustum.points.l_u_n += nearPlaneOffset;
        m_sunShadowFrustum.points.r_u_n += nearPlaneOffset;

        //coarse frustum culling for shadow rendering, assuming shadow frustum if fitted to camera frustum
        //actual frustum is fitted tightly to depth buffer values, but that is done on the GPU
        for (const RenderObject& obj : scene) {

            const bool isVisible = isAxisAlignedBoundingBoxIntersectingViewFrustum(m_sunShadowFrustum, obj.bbWorld);

            if (isVisible) {
                m_currentShadowPassDrawcallCount++;

				const MeshFrontend mesh = m_frontendMeshes[obj.mesh.index];
                culledMeshes.push_back(mesh.backendHandle);

				ShadowPushConstants pushConstants;
				pushConstants.albedoTextureIndex = mesh.material.albedoTextureIndex;
				pushConstants.transformIndex = modelMatrices.size();
				pushConstantData.push_back(pushConstants);

				modelMatrices.push_back(obj.modelMatrix);
            }
        }
        for (uint32_t shadowPass = 0; shadowPass < m_shadowPasses.size(); shadowPass++) {
            gRenderBackend.drawMeshes(culledMeshes, (char*)pushConstantData.data(), m_shadowPasses[shadowPass]);
        }
		gRenderBackend.setStorageBufferData(m_shadowPassTransformsBuffer, modelMatrices.data(),
			sizeof(glm::mat4) * modelMatrices.size());
    }

	if (m_renderBoundingBoxes) {
		std::vector<glm::mat4> boundingBoxMatrices;
		std::vector<MeshHandle> meshHandles;
		std::vector<uint32_t> pushConstantData; //contains index to matrix
		for (const RenderObject& obj : scene) {
			//culling again is repeating work
			//but keeps code nicely separated and is for debugging only
			const bool isVisible = isAxisAlignedBoundingBoxIntersectingViewFrustum(m_cameraFrustum, obj.bbWorld);
			if (isVisible) {
				const glm::vec3 offset = (obj.bbWorld.min + obj.bbWorld.max) * 0.5f;
				const glm::mat4 translationMatrix = glm::translate(glm::mat4(1.f), offset);
				const glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.f), (obj.bbWorld.max - obj.bbWorld.min) * 0.5f);
				const glm::mat4 bbMatrix = m_globalShaderInfo.viewProjection * translationMatrix * scaleMatrix;
				pushConstantData.push_back(boundingBoxMatrices.size());
				boundingBoxMatrices.push_back(bbMatrix);
				meshHandles.push_back(m_boundingBoxMesh);
			}
		}
		gRenderBackend.drawMeshes(meshHandles, (char*)pushConstantData.data(), m_debugGeoPass);
		const size_t bbMatricesSize = boundingBoxMatrices.size() * sizeof(glm::mat4);
		gRenderBackend.setStorageBufferData(m_boundingBoxDebugRenderMatrices, (char*)boundingBoxMatrices.data(), bbMatricesSize);
	}

	//sdf scene
	{
		struct GPUBoundingBox {
			glm::vec3 min; 
			float padding1 = 0.f;
			glm::vec3 max;
			float padding2 = 0.f;
		};
		std::vector<GPUBoundingBox> instanceWorldBBs;
		std::vector<SDFInstance> instanceData;
		instanceData.reserve(scene.size());
		instanceWorldBBs.reserve(scene.size());
		for (const RenderObject& obj : scene) {

			//culling requires the padded bounding box, used for rendering and computation
			const AxisAlignedBoundingBox paddedWorldBB = padSDFBoundingBox(obj.bbWorld);
			GPUBoundingBox worldBB;
			worldBB.min = paddedWorldBB.min;
			worldBB.max = paddedWorldBB.max;
			instanceWorldBBs.push_back(worldBB);

			SDFInstance instance;
			const MeshFrontend& mesh = m_frontendMeshes[obj.mesh.index];
			instance.sdfTextureIndex = mesh.sdfTextureIndex;

			const AxisAlignedBoundingBox paddedLocalBB = padSDFBoundingBox(mesh.localBB);
			instance.localExtends = paddedLocalBB.max - paddedLocalBB.min;
			instance.meanAlbedo = mesh.meanAlbedo;

			const glm::vec3 bbOffset = (paddedLocalBB.min + paddedLocalBB.max) * 0.5f;

			glm::mat4 bbT = glm::translate(glm::mat4x4(1.f), bbOffset);

			instance.worldToLocal = glm::inverse(obj.modelMatrix * bbT);
			instanceData.push_back(instance);
		}
		std::vector<uint8_t> bufferData(sizeof(SDFInstance) * instanceData.size() + sizeof(uint32_t) * 4);
		m_currentSDFInstanceCount = instanceData.size();
		uint32_t padding[3];
		memcpy(bufferData.data(), &m_currentSDFInstanceCount, sizeof(uint32_t));
		memcpy(bufferData.data() + sizeof(uint32_t), &padding, sizeof(uint32_t) * 3);
		memcpy(bufferData.data() + sizeof(uint32_t) * 4, instanceData.data(), sizeof(SDFInstance) * instanceData.size());
		gRenderBackend.setStorageBufferData(m_sdfInstanceBuffer, bufferData.data(), bufferData.size());

		gRenderBackend.setStorageBufferData(m_sdfInstanceWorldBBBuffer, instanceWorldBBs.data(), 
			instanceWorldBBs.size() * sizeof(GPUBoundingBox));
	}
}

void RenderFrontend::renderFrame() {

    if (m_minimized) {
        return;
    }
	m_globalShaderInfo.frameIndex++;

    issueSkyDrawcalls();
    gRenderBackend.renderFrame(true);

    //set after frame finished so logic before rendering can decide if cut should happen
    m_globalShaderInfo.cameraCut = false;
}

void RenderFrontend::computeColorBufferHistogram(const ImageHandle lastFrameColor) const {

    StorageBufferResource histogramPerTileResource(m_histogramPerTileBuffer, false, 0);
    StorageBufferResource histogramResource(m_histogramBuffer, false, 1);

    //histogram per tile
    {
        ImageResource colorTextureResource(lastFrameColor, 0, 2);
        StorageBufferResource lightBufferResource(m_lightBuffer, true, 3);

        RenderPassExecution histogramPerTileExecution;
        histogramPerTileExecution.handle = m_histogramPerTilePass;
        histogramPerTileExecution.resources.storageBuffers = { histogramPerTileResource, lightBufferResource };
        histogramPerTileExecution.resources.sampledImages = { colorTextureResource };
        histogramPerTileExecution.dispatchCount[0] = uint32_t(std::ceilf((float)m_screenWidth / float(histogramTileSizeX)));
        histogramPerTileExecution.dispatchCount[1] = uint32_t(std::ceilf((float)m_screenHeight / float(histogramTileSizeY)));
        histogramPerTileExecution.dispatchCount[2] = 1;

        gRenderBackend.setRenderPassExecution(histogramPerTileExecution);
    }

    const float binsPerDispatch = 64.f;
    //reset global tile
    {
        RenderPassExecution histogramResetExecution;
        histogramResetExecution.handle = m_histogramResetPass;
        histogramResetExecution.resources.storageBuffers = { histogramResource };
        histogramResetExecution.dispatchCount[0] = uint32_t(std::ceilf(float(nHistogramBins) / binsPerDispatch));
        histogramResetExecution.dispatchCount[1] = 1;
        histogramResetExecution.dispatchCount[2] = 1;

        gRenderBackend.setRenderPassExecution(histogramResetExecution);
    }
    //combine tiles
    {
        RenderPassExecution histogramCombineTilesExecution;
        histogramCombineTilesExecution.handle = m_histogramCombinePass;
        histogramCombineTilesExecution.resources.storageBuffers = { histogramPerTileResource, histogramResource };
        uint32_t tileCount =
            (uint32_t)std::ceilf(m_screenWidth / float(histogramTileSizeX)) *
            (uint32_t)std::ceilf(m_screenHeight / float(histogramTileSizeY));
        histogramCombineTilesExecution.dispatchCount[0] = tileCount;
        histogramCombineTilesExecution.dispatchCount[1] = uint32_t(std::ceilf(float(nHistogramBins) / binsPerDispatch));
        histogramCombineTilesExecution.dispatchCount[2] = 1;
        histogramCombineTilesExecution.parents = { m_histogramPerTilePass, m_histogramResetPass };

        gRenderBackend.setRenderPassExecution(histogramCombineTilesExecution);
    }
}
   
void RenderFrontend::renderSky(const FramebufferHandle framebuffer, const RenderPassHandle parent) const {
    gRenderBackend.setUniformBufferData(
        m_atmosphereSettingsBuffer, 
        &m_atmosphereSettings, 
        sizeof(m_atmosphereSettings));
    //compute transmission lut
    {
        ImageResource lutResource(m_skyTransmissionLut, 0, 0);
        UniformBufferResource atmosphereBufferResource(m_atmosphereSettingsBuffer, 1);

        RenderPassExecution skyTransmissionLutExecution;
        skyTransmissionLutExecution.handle = m_skyTransmissionLutPass;
        skyTransmissionLutExecution.resources.storageImages = { lutResource };
        skyTransmissionLutExecution.resources.uniformBuffers = { atmosphereBufferResource };
        skyTransmissionLutExecution.dispatchCount[0] = skyTransmissionLutResolution / 8;
        skyTransmissionLutExecution.dispatchCount[1] = skyTransmissionLutResolution / 8;
        skyTransmissionLutExecution.dispatchCount[2] = 1;
        gRenderBackend.setRenderPassExecution(skyTransmissionLutExecution);
    }
    //compute multiscatter lut
    {
        ImageResource multiscatterLutResource(m_skyMultiscatterLut, 0, 0);
        ImageResource transmissionLutResource(m_skyTransmissionLut, 0, 1);
        UniformBufferResource atmosphereBufferResource(m_atmosphereSettingsBuffer, 3);

        RenderPassExecution skyMultiscatterLutExecution;
        skyMultiscatterLutExecution.handle = m_skyMultiscatterLutPass;
        skyMultiscatterLutExecution.parents = { m_skyTransmissionLutPass };
        skyMultiscatterLutExecution.resources.storageImages = { multiscatterLutResource };
        skyMultiscatterLutExecution.resources.sampledImages = { transmissionLutResource };
        skyMultiscatterLutExecution.resources.uniformBuffers = { atmosphereBufferResource };
        skyMultiscatterLutExecution.dispatchCount[0] = skyMultiscatterLutResolution / 8;
        skyMultiscatterLutExecution.dispatchCount[1] = skyMultiscatterLutResolution / 8;
        skyMultiscatterLutExecution.dispatchCount[2] = 1;
        gRenderBackend.setRenderPassExecution(skyMultiscatterLutExecution);
    }
    //compute sky lut
    {
        ImageResource lutResource(m_skyLut, 0, 0);
        ImageResource lutTransmissionResource(m_skyTransmissionLut, 0, 1);
        ImageResource lutMultiscatterResource(m_skyMultiscatterLut, 0, 2);
        UniformBufferResource atmosphereBufferResource(m_atmosphereSettingsBuffer, 4);
        StorageBufferResource lightBufferResource(m_lightBuffer, true, 5);

        RenderPassExecution skyLutExecution;
        skyLutExecution.handle = m_skyLutPass;
        skyLutExecution.resources.storageImages = { lutResource };
        skyLutExecution.resources.sampledImages = { lutTransmissionResource, lutMultiscatterResource };
        skyLutExecution.resources.uniformBuffers = { atmosphereBufferResource };
        skyLutExecution.resources.storageBuffers = { lightBufferResource };
        skyLutExecution.dispatchCount[0] = skyLutWidth / 8;
        skyLutExecution.dispatchCount[1] = skyLutHeight / 8;
        skyLutExecution.dispatchCount[2] = 1;
        skyLutExecution.parents = { m_skyTransmissionLutPass, m_skyMultiscatterLutPass, m_preExposeLightsPass };
        gRenderBackend.setRenderPassExecution(skyLutExecution);
    }
    //render skybox
    {
        const ImageResource skyLutResource (m_skyLut, 0, 0);

        RenderPassExecution skyPassExecution;
        skyPassExecution.handle = m_skyPass;
        skyPassExecution.framebuffer = framebuffer;
        skyPassExecution.resources.sampledImages = { skyLutResource };
        skyPassExecution.parents = { parent,  m_skyLutPass };
        gRenderBackend.setRenderPassExecution(skyPassExecution);
    }
    //sun sprite
    {
        const StorageBufferResource lightBufferResource(m_lightBuffer, true, 0);
        const ImageResource transmissionLutResource(m_skyTransmissionLut, 0, 1);

        RenderPassExecution sunSpritePassExecution;
        sunSpritePassExecution.handle = m_sunSpritePass;
        sunSpritePassExecution.framebuffer = framebuffer;
        sunSpritePassExecution.parents = { m_skyPass };
        sunSpritePassExecution.resources.storageBuffers = { lightBufferResource };
        sunSpritePassExecution.resources.sampledImages = { transmissionLutResource };
        gRenderBackend.setRenderPassExecution(sunSpritePassExecution);
    }
}

void RenderFrontend::renderSunShadowCascades() const {
    for (uint32_t i = 0; i < shadowCascadeCount; i++) {
        RenderPassExecution shadowPassExecution;
        shadowPassExecution.handle = m_shadowPasses[i];
        shadowPassExecution.parents = { m_lightMatrixPass };
        shadowPassExecution.framebuffer = m_shadowCascadeFramebuffers[i];
        shadowPassExecution.resources.storageBuffers = { 
			StorageBufferResource(m_sunShadowInfoBuffer, true, 0),
			StorageBufferResource(m_shadowPassTransformsBuffer, true, 1)
		};

        gRenderBackend.setRenderPassExecution(shadowPassExecution);
    }
}

void RenderFrontend::computeExposure() const {
    StorageBufferResource lightBufferResource(m_lightBuffer, false, 0);
    StorageBufferResource histogramResource(m_histogramBuffer, false, 1);
    ImageResource transmissionLutResource(m_skyTransmissionLut, 0, 2);

    RenderPassExecution preExposeLightsExecution;
    preExposeLightsExecution.handle = m_preExposeLightsPass;
    preExposeLightsExecution.resources.storageBuffers = { histogramResource, lightBufferResource };
    preExposeLightsExecution.resources.sampledImages = { transmissionLutResource };
    preExposeLightsExecution.parents = { m_histogramCombinePass, m_skyTransmissionLutPass };
    preExposeLightsExecution.dispatchCount[0] = 1;
    preExposeLightsExecution.dispatchCount[1] = 1;
    preExposeLightsExecution.dispatchCount[2] = 1;

    gRenderBackend.setRenderPassExecution(preExposeLightsExecution);
}

void RenderFrontend::renderDepthPrepass(const FramebufferHandle framebuffer) const {
    RenderPassExecution prepassExe;
    prepassExe.handle = m_depthPrePass;
    prepassExe.framebuffer = framebuffer;
	prepassExe.resources.storageBuffers = { StorageBufferResource(m_mainPassTransformsBuffer, true, 0) };
    gRenderBackend.setRenderPassExecution(prepassExe);
}

void RenderFrontend::computeDepthPyramid(const ImageHandle depthBuffer) const {
    RenderPassExecution exe;
    exe.handle = m_depthPyramidPass;
    exe.parents = { m_depthPrePass };

    const uint32_t width = m_screenWidth / 2;
    const uint32_t height = m_screenHeight / 2;
    const uint32_t maxMipCount = 11;

    const uint32_t mipCount = mipCountFromResolution(width, height, 1);
    if (mipCount > maxMipCount) {
        std::cout << "Warning: depth pyramid mip count exceeds calculation shader max\n";
    }

    const glm::ivec2 dispatchCount = computeSinglePassMipChainDispatchCount(width, height, mipCount, maxMipCount);
    exe.dispatchCount[0] = dispatchCount.x;
    exe.dispatchCount[1] = dispatchCount.y;
    exe.dispatchCount[2] = 1;

    ImageResource depthBufferResource(depthBuffer, 0, 13);
    ImageResource depthPyramidResource(m_minMaxDepthPyramid, 0, 15);

    exe.resources.sampledImages = { depthBufferResource, depthPyramidResource };

    StorageBufferResource syncBuffer(m_depthPyramidSyncBuffer, false, 16);
    exe.resources.storageBuffers = { syncBuffer };

    exe.resources.storageImages.reserve(maxMipCount);
    const uint32_t unusedMipCount = maxMipCount - mipCount;
    for (uint32_t i = 0; i < maxMipCount; i++) {
        const uint32_t mipLevel = i >= unusedMipCount ? i - unusedMipCount : 0;
        ImageResource pyramidMip(m_minMaxDepthPyramid, mipLevel, i);
        exe.resources.storageImages.push_back(pyramidMip);
    }
    gRenderBackend.setRenderPassExecution(exe);
}

void RenderFrontend::computeSunLightMatrices() const{
    RenderPassExecution exe;
    exe.handle = m_lightMatrixPass;
    exe.parents = { m_depthPyramidPass };
    exe.dispatchCount[0] = 1;
    exe.dispatchCount[1] = 1;
    exe.dispatchCount[2] = 1;

    const uint32_t depthPyramidMipCount = mipCountFromResolution(m_screenWidth / 2, m_screenHeight / 2, 1);
    ImageResource depthPyramidLowestMipResource(m_minMaxDepthPyramid, depthPyramidMipCount - 1, 1);
    exe.resources.storageImages = { depthPyramidLowestMipResource };

    StorageBufferResource lightMatrixBuffer(m_sunShadowInfoBuffer, false, 0);
    exe.resources.storageBuffers = { lightMatrixBuffer };

    gRenderBackend.setRenderPassExecution(exe);
}

void RenderFrontend::diffuseSDFTrace(const FrameRenderTargets& currentTarget) const {
	
	sdfInstanceCulling(m_sdfTraceInfluenceRadius);

	RenderPassExecution exe;
	exe.handle = m_diffuseSDFTracePass;
	exe.parents = { m_depthPrePass, m_sdfCameraTileCulling, m_sdfShadowTileCulling };
	if (m_shadingConfig.indirectLightingHalfRes) {
		exe.parents = { m_depthDownscalePass };
	}

	exe.resources.storageImages = {
		ImageResource(m_indirectDiffuse_Y_SH[0], 0, 0),
		ImageResource(m_indirectDiffuse_CoCg[0], 0, 1)
	};

	ImageHandle depthSrc = m_shadingConfig.indirectLightingHalfRes ? m_depthHalfRes : currentTarget.depthBuffer;

	exe.resources.sampledImages = {
		ImageResource(depthSrc, 0, 2),
		ImageResource(m_worldSpaceNormalImage, 0, 3),
		ImageResource(m_skyLut, 0, 4)
	};
	exe.resources.storageBuffers = {
		StorageBufferResource(m_lightBuffer, true, 5),
		StorageBufferResource(m_sdfInstanceBuffer, true, 6),
		StorageBufferResource(m_sdfCameraCulledTiles, true, 7),
		StorageBufferResource(m_sdfShadowCulledTiles, true, 8)
	};

	exe.resources.uniformBuffers = {
		UniformBufferResource(m_shadowFrustumInfoBuffer, 9)
	};

	const int resolutionDivider = m_shadingConfig.indirectLightingHalfRes ? 2 : 1;

	const float localThreadSize = 8.f;
	const glm::ivec2 dispatchCount = glm::ivec2(glm::ceil(glm::vec2(
		m_screenWidth / resolutionDivider, 
		m_screenHeight / resolutionDivider) / localThreadSize));
	exe.dispatchCount[0] = dispatchCount.x;
	exe.dispatchCount[1] = dispatchCount.y;
	exe.dispatchCount[2] = 1;

	gRenderBackend.setRenderPassExecution(exe);
}

void RenderFrontend::filterIndirectDiffuse(const FrameRenderTargets& currentFrame, const FrameRenderTargets& lastFrame) const {

	const int resolutionDivider = m_shadingConfig.indirectLightingHalfRes ? 2 : 1;
	const glm::vec2 workingResolution = glm::vec2(m_screenWidth / resolutionDivider, m_screenHeight / resolutionDivider);

	const ImageHandle depthSrc = m_shadingConfig.indirectLightingHalfRes ? m_depthHalfRes : currentFrame.depthBuffer;

	//spatial filter on input
	{
		RenderPassExecution exe;
		exe.handle = m_indirectDiffuseFilterSpatialPass[0];
		exe.parents = { m_diffuseSDFTracePass };

		exe.resources.storageImages = {
			ImageResource(m_indirectDiffuse_Y_SH[1], 0, 0),
			ImageResource(m_indirectDiffuse_CoCg[1], 0, 1)
		};
		exe.resources.sampledImages = {
			ImageResource(m_indirectDiffuse_Y_SH[0], 0, 2),
			ImageResource(m_indirectDiffuse_CoCg[0], 0, 3),
			ImageResource(depthSrc, 0, 4),
			ImageResource(m_worldSpaceNormalImage, 0, 5),
		};

		const float localThreadSize = 8.f;
		const glm::ivec2 dispatchCount = glm::ivec2(glm::ceil(workingResolution / localThreadSize));
		exe.dispatchCount[0] = dispatchCount.x;
		exe.dispatchCount[1] = dispatchCount.y;
		exe.dispatchCount[2] = 1;

		gRenderBackend.setRenderPassExecution(exe);
	}
	//temporal filter
	{
		RenderPassExecution exe;
		exe.handle = m_indirectDiffuseFilterTemporalPass;
		exe.parents = { m_indirectDiffuseFilterSpatialPass[0] };

		const uint32_t historySrcIndex = m_globalShaderInfo.frameIndex % 2;
		const uint32_t historyDstIndex = historySrcIndex == 0 ? 1 : 0;

		exe.resources.storageImages = {
			ImageResource(m_indirectDiffuse_Y_SH[0], 0, 0),
			ImageResource(m_indirectDiffuse_CoCg[0], 0, 1),
			ImageResource(m_indirectDiffuseHistory_Y_SH[1], 0, 2),
			ImageResource(m_indirectDiffuseHistory_CoCg[1], 0, 3)
		};
		exe.resources.sampledImages = {
			ImageResource(m_indirectDiffuse_Y_SH[1], 0, 4),
			ImageResource(m_indirectDiffuse_CoCg[1], 0, 5),
			ImageResource(m_indirectDiffuseHistory_Y_SH[0], 0, 6),
			ImageResource(m_indirectDiffuseHistory_CoCg[0], 0, 7),
			ImageResource(currentFrame.motionBuffer, 0, 8),
			ImageResource(lastFrame.motionBuffer, 0, 9)
		};

		const float localThreadSize = 8.f;
		const glm::ivec2 dispatchCount = glm::ivec2(glm::ceil(workingResolution / localThreadSize));
		exe.dispatchCount[0] = dispatchCount.x;
		exe.dispatchCount[1] = dispatchCount.y;
		exe.dispatchCount[2] = 1;

		gRenderBackend.setRenderPassExecution(exe);
	}
	//spatial filter on history
	{
		RenderPassExecution exe;
		exe.handle = m_indirectDiffuseFilterSpatialPass[1];
		exe.parents = { m_indirectDiffuseFilterTemporalPass };

		exe.resources.storageImages = {
			ImageResource(m_indirectDiffuseHistory_Y_SH[0], 0, 0),
			ImageResource(m_indirectDiffuseHistory_CoCg[0], 0, 1)
		};
		exe.resources.sampledImages = {
			ImageResource(m_indirectDiffuseHistory_Y_SH[1], 0, 2),
			ImageResource(m_indirectDiffuseHistory_CoCg[1], 0, 3),
			ImageResource(depthSrc, 0, 4),
			ImageResource(m_worldSpaceNormalImage, 0, 5),
		};

		const float localThreadSize = 8.f;
		const glm::ivec2 dispatchCount = glm::ivec2(glm::ceil(workingResolution / localThreadSize));
		exe.dispatchCount[0] = dispatchCount.x;
		exe.dispatchCount[1] = dispatchCount.y;
		exe.dispatchCount[2] = 1;

		gRenderBackend.setRenderPassExecution(exe);
	}
	//upscale
	if(m_shadingConfig.indirectLightingHalfRes) {
		RenderPassExecution exe;
		exe.handle = m_indirectLightingUpscale;
		exe.parents = { m_indirectDiffuseFilterSpatialPass[1] };

		exe.resources.storageImages = {
			ImageResource(m_indirectLightingFullRes_Y_SH, 0, 0),
			ImageResource(m_indirectLightingFullRes_CoCg, 0, 1),
		};
		exe.resources.sampledImages = {
			ImageResource(m_indirectDiffuseHistory_Y_SH[0], 0, 2),
			ImageResource(m_indirectDiffuseHistory_CoCg[0], 0, 3),
			ImageResource(currentFrame.depthBuffer, 0, 4),
			ImageResource(m_depthHalfRes, 0, 5)
		};

		const float localThreadSize = 8.f;
		exe.dispatchCount[0] = glm::ceil(m_screenWidth / localThreadSize);
		exe.dispatchCount[1] = glm::ceil(m_screenHeight / localThreadSize);
		exe.dispatchCount[2] = 1;

		gRenderBackend.setRenderPassExecution(exe);
	}
}

void RenderFrontend::downscaleDepth(const FrameRenderTargets& currentTarget) const {
	RenderPassExecution exe;
	exe.handle = m_depthDownscalePass;
	exe.parents = { m_depthPrePass };

	const float localThreadSize = 8.f;
	const glm::vec2 halfRes = glm::ivec2(m_screenWidth, m_screenHeight) / 2;
	const glm::ivec2 dispatchCount = glm::ivec2(glm::ceil(halfRes / localThreadSize));
	exe.dispatchCount[0] = dispatchCount.x;
	exe.dispatchCount[1] = dispatchCount.y;
	exe.dispatchCount[2] = 1;
	
	exe.resources.storageImages = {
		ImageResource(m_depthHalfRes, 0, 0)
	};
	exe.resources.sampledImages = {
		ImageResource(currentTarget.depthBuffer, 0, 1)
	};

	gRenderBackend.setRenderPassExecution(exe);
}

void RenderFrontend::renderForwardShading(const std::vector<RenderPassHandle>& externalDependencies, const FramebufferHandle framebuffer) const {
    const auto diffuseProbeResource = ImageResource(m_diffuseSkyProbe, 0, 1);
    const auto brdfLutResource = ImageResource(m_brdfLut, 0, 3);
    const auto specularProbeResource = ImageResource(m_specularSkyProbe, 0, 4);
    const auto lightBufferResource = StorageBufferResource(m_lightBuffer, true, 7);
    const auto lightMatrixBuffer = StorageBufferResource(m_sunShadowInfoBuffer, true, 8);

    RenderPassExecution mainPassExecution;
    mainPassExecution.handle = m_mainPass;
    mainPassExecution.framebuffer = framebuffer;
    mainPassExecution.resources.storageBuffers = { lightBufferResource, lightMatrixBuffer,
		StorageBufferResource{m_mainPassTransformsBuffer, true, 17} };

	ImageHandle indirectSrc_Y_SH = m_indirectDiffuseHistory_Y_SH[0];
	ImageHandle indirectSrc_CoCg = m_indirectDiffuseHistory_CoCg[0];
	if (m_shadingConfig.indirectLightingHalfRes) {
		indirectSrc_Y_SH = m_indirectLightingFullRes_Y_SH;
		indirectSrc_CoCg = m_indirectLightingFullRes_CoCg;
	}

	mainPassExecution.resources.sampledImages = { diffuseProbeResource, brdfLutResource, specularProbeResource,
		ImageResource(indirectSrc_Y_SH, 0, 15),
		ImageResource(indirectSrc_CoCg, 0, 16) };

    //add shadow map cascade resources
    for (uint32_t i = 0; i < shadowCascadeCount; i++) {
        const auto shadowMapResource = ImageResource(m_shadowMaps[i], 0, 9 + i);
        mainPassExecution.resources.sampledImages.push_back(shadowMapResource);
    }

    mainPassExecution.parents = { m_preExposeLightsPass, m_depthPrePass, m_lightMatrixPass };
    mainPassExecution.parents.insert(mainPassExecution.parents.end(), m_shadowPasses.begin(), m_shadowPasses.end());
    mainPassExecution.parents.insert(mainPassExecution.parents.begin(), externalDependencies.begin(), externalDependencies.end());

    gRenderBackend.setRenderPassExecution(mainPassExecution);
}

void RenderFrontend::copyHDRImage(const ImageHandle src, const ImageHandle dst, RenderPassHandle parent) const {
    const ImageResource srcResource(src, 0, 1);
    const ImageResource dstResource(dst, 0, 2);

    RenderPassExecution exe;
    exe.handle = m_hdrImageCopyPass;
    exe.resources.sampledImages = { srcResource };
    exe.resources.storageImages = { dstResource };
    exe.dispatchCount[0] = (uint32_t)std::ceil(m_screenWidth / 8.f);
    exe.dispatchCount[1] = (uint32_t)std::ceil(m_screenHeight / 8.f);
    exe.dispatchCount[2] = 1;
    exe.parents = { parent };

    gRenderBackend.setRenderPassExecution(exe);
}

void RenderFrontend::computeTemporalSuperSampling(const FrameRenderTargets& currentFrame, const FrameRenderTargets& lastFrame, 
    const ImageHandle target, const RenderPassHandle parent) const {
    //scene luminance
    {
        const ImageResource srcResource(currentFrame.colorBuffer, 0, 0);
        const ImageResource dstResource(m_sceneLuminance, 0, 1);

        RenderPassExecution exe;
        exe.handle = m_colorToLuminancePass;
        exe.resources.storageImages = { dstResource };
        exe.resources.sampledImages = { srcResource };
        exe.dispatchCount[0] = (uint32_t)std::ceil(m_screenWidth / 8.f);
        exe.dispatchCount[1] = (uint32_t)std::ceil(m_screenHeight / 8.f);
        exe.dispatchCount[2] = 1;
        exe.parents = { parent };

        gRenderBackend.setRenderPassExecution(exe);
    }
    //temporal supersampling
    {
        const ImageResource currentFrameResource(currentFrame.colorBuffer, 0, 1);
        const ImageResource lastFrameResource(lastFrame.colorBuffer, 0, 2);
        const ImageResource targetResource(target, 0, 3);
        const ImageResource velocityBufferResource(currentFrame.motionBuffer, 0, 4);
        const ImageResource currentDepthResource(currentFrame.depthBuffer, 0, 5);
        const ImageResource lastDepthResource(lastFrame.depthBuffer, 0, 6);
        const ImageResource currentLuminanceResource(m_sceneLuminance, 0, 7);
        const ImageResource lastLuminanceResource(m_lastFrameLuminance, 0, 8);

        RenderPassExecution temporalSupersamplingExecution;
        temporalSupersamplingExecution.handle = m_temporalSupersamplingPass;
        temporalSupersamplingExecution.resources.storageImages = { targetResource };
        temporalSupersamplingExecution.resources.sampledImages = {
            currentFrameResource,
            lastFrameResource,
            velocityBufferResource,
            currentDepthResource,
            lastDepthResource,
            currentLuminanceResource,
            lastLuminanceResource };
        temporalSupersamplingExecution.dispatchCount[0] = (uint32_t)std::ceil(m_screenWidth / 8.f);
        temporalSupersamplingExecution.dispatchCount[1] = (uint32_t)std::ceil(m_screenHeight / 8.f);
        temporalSupersamplingExecution.dispatchCount[2] = 1;
        temporalSupersamplingExecution.parents = { m_colorToLuminancePass };

        gRenderBackend.setRenderPassExecution(temporalSupersamplingExecution);
    }
}

void RenderFrontend::computeTemporalFilter(const ImageHandle colorSrc, const FrameRenderTargets& currentFrame, const ImageHandle target, const RenderPassHandle parent, 
    const ImageHandle historyBufferSrc, const ImageHandle historyBufferDst) const {

    const ImageResource inputImageResource(colorSrc, 0, 0);
    const ImageResource outputImageResource(target, 0, 1);
    const ImageResource historyDstResource(historyBufferDst, 0, 2);
    const ImageResource historySrcResource(historyBufferSrc, 0, 3);
    const ImageResource motionBufferResource(currentFrame.motionBuffer, 0, 4);
    const ImageResource depthBufferResource(currentFrame.depthBuffer, 0, 5);
    const UniformBufferResource resolveWeightsResource(m_taaResolveWeightBuffer, 6);

    RenderPassExecution temporalFilterExecution;
    temporalFilterExecution.handle = m_temporalFilterPass;
    temporalFilterExecution.resources.storageImages = { outputImageResource, historyDstResource };
    temporalFilterExecution.resources.sampledImages = { inputImageResource, historySrcResource, motionBufferResource, depthBufferResource };
    temporalFilterExecution.resources.uniformBuffers = { resolveWeightsResource };
    temporalFilterExecution.dispatchCount[0] = (uint32_t)std::ceil(m_screenWidth / 8.f);
    temporalFilterExecution.dispatchCount[1] = (uint32_t)std::ceil(m_screenHeight / 8.f);
    temporalFilterExecution.dispatchCount[2] = 1;
    temporalFilterExecution.parents = { parent };

    gRenderBackend.setRenderPassExecution(temporalFilterExecution);
}

void RenderFrontend::computeTonemapping(const RenderPassHandle parent, const ImageHandle& src) const {
    const auto swapchainInput = gRenderBackend.getSwapchainInputImage();
    ImageResource targetResource(swapchainInput, 0, 0);
    ImageResource colorBufferResource(src, 0, 1);

    RenderPassExecution tonemappingExecution;
    tonemappingExecution.handle = m_tonemappingPass;
    tonemappingExecution.resources.storageImages = { targetResource };
    tonemappingExecution.resources.sampledImages = { colorBufferResource };
    tonemappingExecution.dispatchCount[0] = (uint32_t)std::ceil(m_screenWidth / 8.f);
    tonemappingExecution.dispatchCount[1] = (uint32_t)std::ceil(m_screenHeight / 8.f);
    tonemappingExecution.dispatchCount[2] = 1;
    tonemappingExecution.parents = { parent };

    gRenderBackend.setRenderPassExecution(tonemappingExecution);
}

void RenderFrontend::renderDebugGeometry(const FramebufferHandle framebuffer) const {
    RenderPassExecution debugPassExecution;
    debugPassExecution.handle = m_debugGeoPass;
    debugPassExecution.framebuffer = framebuffer;
    debugPassExecution.parents = { m_mainPass };
	debugPassExecution.resources.storageBuffers = { StorageBufferResource(m_boundingBoxDebugRenderMatrices, true, 0) };
    gRenderBackend.setRenderPassExecution(debugPassExecution);
}

void RenderFrontend::issueSkyDrawcalls() {
    gRenderBackend.drawMeshes(std::vector<MeshHandle> {m_skyCube}, nullptr, m_skyPass);

    const float lattitudeOffsetAngle = 90;
    const float longitudeOffsetAngle = -90;
    const float sunAngularDiameter = 0.535f; //from "Physically Based Sky, Atmosphereand Cloud Rendering in Frostbite", page 25
    const float spriteScale = glm::tan(glm::radians(sunAngularDiameter * 0.5f));
    const glm::mat4 spriteScaleMatrix = glm::scale(glm::mat4(1.f), glm::vec3(spriteScale, spriteScale, 1.f));
    const glm::mat4 spriteLattitudeRotation = glm::rotate(glm::mat4(1.f), glm::radians(m_sunDirection.y + lattitudeOffsetAngle), glm::vec3(-1, 0, 0));
    const glm::mat4 spriteLongitudeRotation = glm::rotate(glm::mat4(1.f), glm::radians(m_sunDirection.x + longitudeOffsetAngle), glm::vec3(0, -1, 0));

	struct SunSpriteMatrices {
		glm::mat4 model;
		glm::mat4 mvp;
	};
	SunSpriteMatrices sunSpriteMatrices;
	sunSpriteMatrices.model = spriteLongitudeRotation * spriteLattitudeRotation * spriteScaleMatrix;
	sunSpriteMatrices.mvp = m_viewProjectionMatrix * sunSpriteMatrices.model;

    gRenderBackend.drawMeshes(std::vector<MeshHandle> {m_quad}, (char*)&sunSpriteMatrices, m_sunSpritePass);
}

void RenderFrontend::renderSDFDebug(const std::vector<RenderPassHandle> parent) const{

	sdfInstanceCulling(0.f); //only instances directly visible within view cone required -> set influence radius to zero

	RenderPassExecution exe;
	exe.handle = m_sdfDebugPass;
	exe.resources.storageImages = { ImageResource(m_postProcessBuffers[0], 0, 0) };
	exe.resources.sampledImages = { ImageResource(m_skyLut, 0, 2) };
	exe.resources.storageBuffers = {
		StorageBufferResource(m_lightBuffer, true, 1),
		StorageBufferResource(m_sdfInstanceBuffer, true, 3),
		StorageBufferResource(m_sdfCameraCulledTiles, true, 4),
		StorageBufferResource(m_sdfShadowCulledTiles, true, 5),
		StorageBufferResource(m_sdfCameraFrustumCulledInstances, true, 6),
		StorageBufferResource(m_sdfShadowFrustumCulledInstances, true, 7),			
	};
	exe.resources.uniformBuffers = {
		UniformBufferResource(m_shadowFrustumInfoBuffer, 8)
	};
	exe.dispatchCount[0] = (uint32_t)std::ceil(m_screenWidth / 8.f);
	exe.dispatchCount[1] = (uint32_t)std::ceil(m_screenHeight / 8.f);
	exe.dispatchCount[2] = 1;
	exe.parents = parent;
	exe.parents.push_back(m_sdfCameraTileCulling);
	exe.parents.push_back(m_sdfShadowTileCulling);

	gRenderBackend.setRenderPassExecution(exe);
}

void RenderFrontend::sdfInstanceCulling(const float sdfInfluenceRadius) const{
	//camera frustum culling
	{
		struct GPUFrustumData {
			glm::vec4 points[6];
			glm::vec4 normal[6];
		};
		GPUFrustumData frustumData;
		//top
		frustumData.points[0] = glm::vec4(m_cameraFrustum.points.l_u_f, 0);
		frustumData.normal[0] = glm::vec4(m_cameraFrustum.normals.top, 0);
		//bot
		frustumData.points[1] = glm::vec4(m_cameraFrustum.points.l_l_f, 0);
		frustumData.normal[1] = glm::vec4(m_cameraFrustum.normals.bot, 0);
		//near
		frustumData.points[2] = glm::vec4(m_cameraFrustum.points.l_l_n, 0);
		frustumData.normal[2] = glm::vec4(m_cameraFrustum.normals.near, 0);
		//far
		frustumData.points[3] = glm::vec4(m_cameraFrustum.points.l_l_f, 0);
		frustumData.normal[3] = glm::vec4(m_cameraFrustum.normals.far, 0);
		//left
		frustumData.points[4] = glm::vec4(m_cameraFrustum.points.l_l_f, 0);
		frustumData.normal[4] = glm::vec4(m_cameraFrustum.normals.left, 0);
		//right
		frustumData.points[5] = glm::vec4(m_cameraFrustum.points.r_l_f, 0);
		frustumData.normal[5] = glm::vec4(m_cameraFrustum.normals.right, 0);

		gRenderBackend.setUniformBufferData(m_cameraFrustumBuffer, &frustumData, sizeof(frustumData));

		//reset culled counter
		uint32_t zero = 0;
		gRenderBackend.setStorageBufferData(m_sdfCameraFrustumCulledInstances, &zero, sizeof(zero));

		RenderPassExecution exe;
		exe.handle = m_sdfCameraFrustumCulling;
		exe.resources.storageBuffers = {
			StorageBufferResource(m_sdfInstanceBuffer, true, 0),
			StorageBufferResource(m_sdfCameraFrustumCulledInstances, false, 2),
			StorageBufferResource(m_sdfInstanceWorldBBBuffer, true, 3)
		};
		exe.resources.uniformBuffers = {
			UniformBufferResource(m_cameraFrustumBuffer, 1)
		};
		exe.dispatchCount[0] = uint32_t(glm::ceil(m_currentSDFInstanceCount / 64.f));
		exe.dispatchCount[1] = 1;
		exe.dispatchCount[2] = 1;

		gRenderBackend.setRenderPassExecution(exe);
	}

	//custom camera and shadow frusta to limit shadow distance
	//with high far plane, shadow culling tiles cover so much space that they are ineffective
	Camera sdfCamera = m_camera;
	sdfCamera.intrinsic.far = m_sdfShadowDistance;
	const ViewFrustum sdfCameraFrustum = computeViewFrustum(sdfCamera);
	const ViewFrustum sdfShadowFrustum = computeOrthogonalFrustumFittedToCamera(sdfCameraFrustum, directionToVector(m_sunDirection));

	//shadow frustum culling
	{
		//no near plane culling, as objects behing near plane can cast shadow into camera frustum
		struct GPUShadowFrustumData {
			glm::vec4 points[5];
			glm::vec4 normal[5];
		};

		GPUShadowFrustumData frustumData;
		//top
		frustumData.points[0] = glm::vec4(sdfShadowFrustum.points.l_u_f, 0);
		frustumData.normal[0] = glm::vec4(sdfShadowFrustum.normals.top, 0);
		//bot
		frustumData.points[1] = glm::vec4(sdfShadowFrustum.points.l_l_f, 0);
		frustumData.normal[1] = glm::vec4(sdfShadowFrustum.normals.bot, 0);
		//far
		frustumData.points[2] = glm::vec4(sdfShadowFrustum.points.l_l_f, 0);
		frustumData.normal[2] = glm::vec4(sdfShadowFrustum.normals.far, 0);
		//left
		frustumData.points[3] = glm::vec4(sdfShadowFrustum.points.l_l_f, 0);
		frustumData.normal[3] = glm::vec4(sdfShadowFrustum.normals.left, 0);
		//right
		frustumData.points[4] = glm::vec4(sdfShadowFrustum.points.r_l_f, 0);
		frustumData.normal[4] = glm::vec4(sdfShadowFrustum.normals.right, 0);

		gRenderBackend.setUniformBufferData(m_shadowFrustumPointsBuffer, &frustumData, sizeof(frustumData));

		//reset culled counter
		uint32_t zero = 0;
		gRenderBackend.setStorageBufferData(m_sdfShadowFrustumCulledInstances, &zero, sizeof(zero));

		RenderPassExecution exe;
		exe.handle = m_sdfShadowFrustumCulling;
		exe.resources.storageBuffers = {
			StorageBufferResource(m_sdfInstanceBuffer, true, 0),
			StorageBufferResource(m_sdfShadowFrustumCulledInstances, false, 2),
			StorageBufferResource(m_sdfInstanceWorldBBBuffer, true, 3)
		};
		exe.resources.uniformBuffers = {
			UniformBufferResource(m_shadowFrustumPointsBuffer, 1)
		};
		exe.dispatchCount[0] = uint32_t(glm::ceil(m_currentSDFInstanceCount / 64.f));
		exe.dispatchCount[1] = 1;
		exe.dispatchCount[2] = 1;

		gRenderBackend.setRenderPassExecution(exe);
	}
	//shadow tile culling
	{
		//compute and set shadow frustum info
		ShadowFrustumInfo shadowFrustum;

		const glm::vec3 L = -directionToVector(m_sunDirection);
		const glm::vec3 up = glm::abs(glm::dot(L, glm::vec3(0.f, -1.f, 0.f))) < 0.99f ? glm::vec3(0.f, -1.f, 0.f) : glm::vec3(-1.f, 0.f, 0.f);
		shadowFrustum.frustumRight = glm::normalize(glm::cross(up, L));
		shadowFrustum.frustumUp = glm::cross(L, shadowFrustum.frustumRight);

		glm::vec2 projectedMin = glm::vec2(INFINITY);
		glm::vec2 projectedMax = glm::vec2(-INFINITY);
		for (const glm::vec3 p : getFrustumPoints(sdfShadowFrustum)) {
			const glm::vec2 projected = glm::vec2(
				glm::dot(p, shadowFrustum.frustumRight),
				glm::dot(p, shadowFrustum.frustumUp));

			projectedMin = glm::min(projectedMin, projected);
			projectedMax = glm::max(projectedMax, projected);
		}

		projectedMin -= sdfInfluenceRadius;
		projectedMax += sdfInfluenceRadius;

		shadowFrustum.frustumExtends = projectedMax - projectedMin;
		shadowFrustum.frustumOffset = projectedMin;

		gRenderBackend.setUniformBufferData(m_shadowFrustumInfoBuffer, &shadowFrustum, sizeof(ShadowFrustumInfo));

		//set execution
		RenderPassExecution exe;
		exe.handle = m_sdfShadowTileCulling;
		exe.parents = { m_sdfShadowFrustumCulling };

		const uint32_t localGroupSize = 8;
		exe.dispatchCount[0] = uint32_t(glm::ceil(sdfShadowCullingTilesCount1D / float(localGroupSize)));
		exe.dispatchCount[1] = uint32_t(glm::ceil(sdfShadowCullingTilesCount1D / float(localGroupSize)));
		exe.dispatchCount[2] = 1;

		exe.resources.storageBuffers = {
			StorageBufferResource(m_sdfShadowFrustumCulledInstances, true, 0),
			StorageBufferResource(m_sdfInstanceWorldBBBuffer, true, 1),
			StorageBufferResource(m_sdfShadowCulledTiles, false, 2)
		};

		exe.resources.uniformBuffers = {
			UniformBufferResource(m_shadowFrustumInfoBuffer, 3)
		};

		gRenderBackend.setRenderPassExecution(exe);
	}
	//camera tile culling
	{
		RenderPassExecution exe;
		exe.handle = m_sdfCameraTileCulling;
		exe.parents = { m_sdfCameraFrustumCulling };

		const uint32_t tileCountX = uint32_t(glm::ceil(m_screenWidth / float(sdfCameraCullingTileSize)));
		const uint32_t tileCountY = uint32_t(glm::ceil(m_screenHeight / float(sdfCameraCullingTileSize)));

		const uint32_t localGroupSize = 8;

		exe.dispatchCount[0] = uint32_t(glm::ceil(tileCountX / float(localGroupSize)));
		exe.dispatchCount[1] = uint32_t(glm::ceil(tileCountY / float(localGroupSize)));
		exe.dispatchCount[2] = 1;

		exe.resources.storageBuffers = {
			StorageBufferResource(m_sdfCameraFrustumCulledInstances, true, 0),
			StorageBufferResource(m_sdfInstanceWorldBBBuffer, true, 1),
			StorageBufferResource(m_sdfCameraCulledTiles, false, 2)
		};

		gRenderBackend.setUniformBufferData(m_sdfTraceInfluenceRangeBuffer, &sdfInfluenceRadius, sizeof(m_sdfTraceInfluenceRadius));

		exe.resources.uniformBuffers = {
			UniformBufferResource(m_sdfTraceInfluenceRangeBuffer, 3)
		};

		gRenderBackend.setRenderPassExecution(exe);
	}
}

void RenderFrontend::updateSceneSDFInfo(const AxisAlignedBoundingBox& sceneBB) {
	AxisAlignedBoundingBox paddedSceneBB = padSDFBoundingBox(sceneBB);
	const VolumeInfo sdfVolumeInfo = volumeInfoFromBoundingBox(paddedSceneBB);
	gRenderBackend.setUniformBufferData(m_sdfVolumeInfoBuffer, &sdfVolumeInfo, sizeof(sdfVolumeInfo));
}

void RenderFrontend::resizeIndirectLightingBuffers() {
	const uint32_t divider = m_shadingConfig.indirectLightingHalfRes ? 2 : 1;
	gRenderBackend.resizeImages({
		m_indirectDiffuse_Y_SH[0],
		m_indirectDiffuse_Y_SH[1],
		m_indirectDiffuse_CoCg[0],
		m_indirectDiffuse_CoCg[1],
		m_indirectDiffuseHistory_Y_SH[0],
		m_indirectDiffuseHistory_Y_SH[1],
		m_indirectDiffuseHistory_CoCg[0],
		m_indirectDiffuseHistory_CoCg[1]
		}, m_screenWidth / divider, m_screenHeight / divider);
}

bool RenderFrontend::loadImageFromPath(std::filesystem::path path, ImageHandle* outImageHandle) {

    if (path == "") {
        return false;
    }

    if (m_textureMap.find(path.string()) == m_textureMap.end()) {
        ImageDescription image;
        if (loadImage(path, true, &image)) {
            *outImageHandle = gRenderBackend.createImage(image);
            m_textureMap[path.string()] = *outImageHandle;
            return true;
        }
        else {
            return false;
        }
    }
    else {
        *outImageHandle = m_textureMap[path.string()];
        return true;
    }
}

void RenderFrontend::skyIBLConvolution() {
    //diffuse convolution
    {
        const auto diffuseProbeResource = ImageResource(m_diffuseSkyProbe, 0, 0);
        const auto skyLutResource = ImageResource(m_skyLut, 0, 1);

        RenderPassExecution diffuseConvolutionExecution;
        diffuseConvolutionExecution.handle = m_skyDiffuseConvolutionPass;
        diffuseConvolutionExecution.parents = { m_skyLutPass };
        diffuseConvolutionExecution.resources.storageImages = { diffuseProbeResource };
        diffuseConvolutionExecution.resources.sampledImages = { skyLutResource };
        diffuseConvolutionExecution.dispatchCount[0] = uint32_t(std::ceil(diffuseSkyProbeRes / 8.f));
        diffuseConvolutionExecution.dispatchCount[1] = uint32_t(std::ceil(diffuseSkyProbeRes / 8.f));
        diffuseConvolutionExecution.dispatchCount[2] = 6;
        gRenderBackend.setRenderPassExecution(diffuseConvolutionExecution);
    }
    //specular probe convolution
    for (uint32_t mipLevel = 0; mipLevel < m_specularSkyProbeMipCount; mipLevel++) {

        const auto specularProbeResource = ImageResource(m_specularSkyProbe, mipLevel, 0);
        const auto skyLutResource = ImageResource(m_skyLut, 0, 1);

        RenderPassExecution specularConvolutionExecution;
        specularConvolutionExecution.handle = m_skySpecularConvolutionPerMipPasses[mipLevel];
        specularConvolutionExecution.parents = { m_skyLutPass };
        specularConvolutionExecution.resources.storageImages = { specularProbeResource };
        specularConvolutionExecution.resources.sampledImages = { skyLutResource };
        specularConvolutionExecution.dispatchCount[0] = specularSkyProbeRes / uint32_t(pow(2, mipLevel)) / 8;
        specularConvolutionExecution.dispatchCount[1] = specularSkyProbeRes / uint32_t(pow(2, mipLevel)) / 8;
        specularConvolutionExecution.dispatchCount[2] = 6;
        gRenderBackend.setRenderPassExecution(specularConvolutionExecution);
    }
}

void RenderFrontend::computeBRDFLut() {

    const auto brdfLutStorageResource = ImageResource(m_brdfLut, 0, 0);

    RenderPassExecution brdfLutExecution;
    brdfLutExecution.handle = m_brdfLutPass;
    brdfLutExecution.resources.storageImages = { brdfLutStorageResource };
    brdfLutExecution.dispatchCount[0] = brdfLutRes / 8;
    brdfLutExecution.dispatchCount[1] = brdfLutRes / 8;
    brdfLutExecution.dispatchCount[2] = 1;
    gRenderBackend.setRenderPassExecution(brdfLutExecution);
}

void RenderFrontend::updateCameraFrustum() {
    m_cameraFrustum = computeViewFrustum(m_camera);

    //debug geo
    std::vector<glm::vec3> frustumPoints;
    std::vector<uint32_t> frustumIndices;

    frustumToLineMesh(m_cameraFrustum, &frustumPoints, &frustumIndices);
    gRenderBackend.updateDynamicMeshes({ m_cameraFrustumModel }, { frustumPoints }, { frustumIndices });
}

void RenderFrontend::updateShadowFrustum() {
    m_sunShadowFrustum = computeOrthogonalFrustumFittedToCamera(m_cameraFrustum, directionToVector(m_sunDirection));

    //debug geo
    std::vector<glm::vec3> frustumPoints;
    std::vector<uint32_t> frustumIndices;
    frustumToLineMesh(m_sunShadowFrustum, &frustumPoints, &frustumIndices);
    gRenderBackend.updateDynamicMeshes({ m_shadowFrustumModel }, { frustumPoints }, { frustumIndices });
}

HistogramSettings RenderFrontend::createHistogramSettings() {
    HistogramSettings settings;

    settings.minValue = 0.001f;
    settings.maxValue = 200000.f;

    uint32_t pixelsPerTile = histogramTileSizeX * histogramTileSizeX;
    settings.maxTileCount = 1920 * 1080 / pixelsPerTile; //FIXME: update buffer on rescale

    return settings;
}

GraphicPassShaderDescriptions RenderFrontend::createForwardPassShaderDescription(const ShadingConfig& config) {

    GraphicPassShaderDescriptions shaderDesc;
    shaderDesc.vertex.srcPathRelative = "triangle.vert";
    shaderDesc.fragment.srcPathRelative = "triangle.frag";

    //specialisation constants
    {
        auto& constants = shaderDesc.fragment.specialisationConstants;

        //diffuse BRDF
        constants.push_back({
            0,                                                                      //location
            dataToCharArray((void*)&config.diffuseBRDF, sizeof(config.diffuseBRDF)) //value
            });
        //direct specular multiscattering
        constants.push_back({
            1,                                                                                      //location
            dataToCharArray((void*)&config.directMultiscatter, sizeof(config.directMultiscatter))   //value
            });
        //use indirect multiscattering
        constants.push_back({
            2,                                                                                              //location
            dataToCharArray((void*)&config.useIndirectMultiscatter, sizeof(config.useIndirectMultiscatter)) //value
            });
        //use geometry AA
        constants.push_back({
            3,                                                                          //location
            dataToCharArray((void*)&config.useGeometryAA, sizeof(config.useGeometryAA)) //value
            });
        //specular probe mip count
        constants.push_back({
            4,																							//location
            dataToCharArray((void*)&m_specularSkyProbeMipCount, sizeof(m_specularSkyProbeMipCount))		//value
            });
		//indirect lighting tech
		constants.push_back({
				5,																											//location
				dataToCharArray((void*)&m_shadingConfig.indirectLightingTech, sizeof(m_shadingConfig.indirectLightingTech)) //value
			});
    }

    return shaderDesc;
}

ShaderDescription RenderFrontend::createBRDFLutShaderDescription(const ShadingConfig& config) {

    ShaderDescription desc;
    desc.srcPathRelative = "brdfLut.comp";

    //diffuse brdf specialisation constant
    desc.specialisationConstants.push_back({
        0,                                                                      //location
        dataToCharArray((void*)&config.diffuseBRDF, sizeof(config.diffuseBRDF)) //value
        });
    
    return desc;
}

ShaderDescription RenderFrontend::createTemporalFilterShaderDescription() {
    ShaderDescription desc;
    desc.srcPathRelative = "temporalFilter.comp";
    
    //specialisation constants
    {
        //use clipping
        desc.specialisationConstants.push_back({
            0,                                                                                                      //location
            dataToCharArray(&m_temporalFilterSettings.useClipping, sizeof(m_temporalFilterSettings.useClipping))    //value
            });
        //use use motion vector dilation
        desc.specialisationConstants.push_back({
            1,                                                                                                                          //location
            dataToCharArray(&m_temporalFilterSettings.useMotionVectorDilation, sizeof(m_temporalFilterSettings.useMotionVectorDilation))//value
            });
        //history sampling tech
        desc.specialisationConstants.push_back({
            2,                                                                                                                  //location
            dataToCharArray(&m_temporalFilterSettings.historySamplingTech, sizeof(m_temporalFilterSettings.historySamplingTech))//value
            });
        //using tonemapping
        desc.specialisationConstants.push_back({
            3,                                                                                                                      //location
            dataToCharArray(&m_temporalFilterSettings.filterUseTonemapping, sizeof(m_temporalFilterSettings.filterUseTonemapping))  //value
            });
    }

    return desc;
}

ShaderDescription RenderFrontend::createTemporalSupersamplingShaderDescription() {
    ShaderDescription desc;
    desc.srcPathRelative = "temporalSupersampling.comp";

    //specialisation constant
    {
        //using tonemapping
        desc.specialisationConstants.push_back({
            0,                                                                                                                                      //location
            dataToCharArray((void*)&m_temporalFilterSettings.supersampleUseTonemapping, sizeof(m_temporalFilterSettings.supersampleUseTonemapping)) //value
            });
    }

    return desc;
}

ShaderDescription RenderFrontend::createIndirectLightingMipCreationShaderDescription() {
	ShaderDescription desc;
	desc.srcPathRelative = "colorMip_rgba16f.comp";

	const int resolutionDivider = m_shadingConfig.indirectLightingHalfRes ? 2 : 1;
	const uint32_t srcWidth = m_screenWidth / resolutionDivider;
	const uint32_t srcHeight = m_screenHeight / resolutionDivider;
	const uint32_t mipCount = glm::min(mipCountFromResolution(srcWidth, srcHeight, 1), indirectLightingMaxMipCount);

	//mip count
	desc.specialisationConstants.push_back({
			0,                                                  //location
			dataToCharArray((void*)&mipCount, sizeof(mipCount)) //value
		});

	return desc;
}

ShaderDescription RenderFrontend::createEdgeMipCreationShaderDescription() {
	ShaderDescription desc;
	desc.srcPathRelative = "colorMip_r8.comp";

	const uint32_t mipCount = glm::min(mipCountFromResolution(m_screenWidth, m_screenHeight, 1), edgeMaxMipCount);

	//mip count
	desc.specialisationConstants.push_back({
			0,                                                  //location
			dataToCharArray((void*)&mipCount, sizeof(mipCount)) //value
		});

	return desc;
}

void RenderFrontend::updateGlobalShaderInfo() {
    m_globalShaderInfo.sunDirection = glm::vec4(directionToVector(m_sunDirection), 0.f);
    m_globalShaderInfo.cameraPos = glm::vec4(m_camera.extrinsic.position, 1.f); 

    m_globalShaderInfo.deltaTime = Timer::getDeltaTimeFloat();
    m_globalShaderInfo.time = Timer::getTimeFloat();
    m_globalShaderInfo.nearPlane = m_camera.intrinsic.near;
    m_globalShaderInfo.farPlane = m_camera.intrinsic.far;

    m_globalShaderInfo.cameraRight      = glm::vec4(m_camera.extrinsic.right, 0);
    m_globalShaderInfo.cameraUp         = glm::vec4(m_camera.extrinsic.up, 0);
    m_globalShaderInfo.cameraForward    = glm::vec4(m_camera.extrinsic.forward, 0);
    m_globalShaderInfo.cameraTanFovHalf = glm::tan(glm::radians(m_camera.intrinsic.fov) * 0.5f);
    m_globalShaderInfo.cameraAspectRatio = m_camera.intrinsic.aspectRatio;
    m_globalShaderInfo.screenResolution = glm::ivec2(m_screenWidth, m_screenHeight);

    //supersampling needs a lod bias as the derivatives are incorrect
    //see "Filmic SMAA", page 117
    //we spread 8 samples over a 2 pixek radius, resulting in an average distance of 2/8 = 0.25
    //the resulting diagonal distance between the samples is sqrt(0.25) = 0.5
    const float lodBiasSampleRadius = 0.5f;
    m_globalShaderInfo.mipBias = m_temporalFilterSettings.enabled && m_temporalFilterSettings.useMipBias ? glm::log2(lodBiasSampleRadius) : 0.f;

    gRenderBackend.setUniformBufferData(m_globalUniformBuffer, &m_globalShaderInfo, sizeof(m_globalShaderInfo));

    m_noiseTextureIndex++;
    m_noiseTextureIndex = m_noiseTextureIndex % m_noiseTextures.size();
}

void RenderFrontend::initImages() {
    //sky occlusion volume is created later
    //its resolution is dependent on scene size in order to fit desired texel density

    //post process buffer
    {
        ImageDescription desc;
        desc.initialData = std::vector<uint8_t>{};
        desc.width = m_screenWidth;
        desc.height = m_screenHeight;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::R11G11B10_uFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 0;
        desc.autoCreateMips = false;

        m_postProcessBuffers[0] = gRenderBackend.createImage(desc);
        m_postProcessBuffers[1] = gRenderBackend.createImage(desc);
    }
    //history buffer for TAA
    {
        ImageDescription desc;
        desc.width = m_screenWidth;
        desc.height = m_screenHeight;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::R11G11B10_uFloat;
        desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_historyBuffers[0] = gRenderBackend.createImage(desc);
        m_historyBuffers[1] = gRenderBackend.createImage(desc);
    }
    //shadow map cascades
    {
        ImageDescription desc;
        desc.width = shadowMapRes;
        desc.height = shadowMapRes;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::Depth16;
        desc.usageFlags = ImageUsageFlags::Attachment | ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_shadowMaps.reserve(shadowCascadeCount);
        for (uint32_t i = 0; i < shadowCascadeCount; i++) {
            const auto shadowMap = gRenderBackend.createImage(desc);
            m_shadowMaps.push_back(shadowMap);
        }
    }
    //specular sky probe
    {
        m_specularSkyProbeMipCount = mipCountFromResolution(specularSkyProbeRes, specularSkyProbeRes, 1);
        //don't use the last few mips as they are too small
        const uint32_t mipsTooSmallCount = 4;
        if (m_specularSkyProbeMipCount > mipsTooSmallCount) {
            m_specularSkyProbeMipCount -= mipsTooSmallCount;
        }

        ImageDescription desc;
        desc.width = specularSkyProbeRes;
        desc.height = specularSkyProbeRes;
        desc.depth = 1;
        desc.type = ImageType::TypeCube;
        desc.format = ImageFormat::R11G11B10_uFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
        desc.mipCount = MipCount::Manual;
        desc.manualMipCount = m_specularSkyProbeMipCount;
        desc.autoCreateMips = false;

        m_specularSkyProbe = gRenderBackend.createImage(desc);
    }
    //diffuse sky probe
    {
        ImageDescription desc;
        desc.width = diffuseSkyProbeRes;
        desc.height = diffuseSkyProbeRes;
        desc.depth = 1;
        desc.type = ImageType::TypeCube;
        desc.format = ImageFormat::R11G11B10_uFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 0;
        desc.autoCreateMips = false;

        m_diffuseSkyProbe = gRenderBackend.createImage(desc);
    }
    //sky cubemap
    {
        ImageDescription desc;
        desc.width = skyTextureRes;
        desc.height = skyTextureRes;
        desc.depth = 1;
        desc.type = ImageType::TypeCube;
        desc.format = ImageFormat::R11G11B10_uFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
        desc.mipCount = MipCount::Manual;
        desc.manualMipCount = 8;
        desc.autoCreateMips = false;

        m_skyTexture = gRenderBackend.createImage(desc);
    }
    //brdf LUT
    {
        ImageDescription desc;
        desc.width = brdfLutRes;
        desc.height = brdfLutRes;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::RGBA16_sFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_brdfLut = gRenderBackend.createImage(desc);
    }
    //sky transmission lut
    {
        ImageDescription desc;
        desc.width = skyTransmissionLutResolution;
        desc.height = skyTransmissionLutResolution;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::R11G11B10_uFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_skyTransmissionLut = gRenderBackend.createImage(desc);
    }
    //sky multiscatter lut
    {
        ImageDescription desc;
        desc.width = skyMultiscatterLutResolution;
        desc.height = skyMultiscatterLutResolution;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::R11G11B10_uFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_skyMultiscatterLut = gRenderBackend.createImage(desc);
    }
    //sky lut
    {
        ImageDescription desc;
        desc.width = skyLutWidth;
        desc.height = skyLutHeight;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::R11G11B10_uFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_skyLut = gRenderBackend.createImage(desc);
    }
    //min/max depth pyramid
    {
        ImageDescription desc;
        desc.autoCreateMips = false;
        desc.width = m_screenWidth / 2;
        desc.height = m_screenHeight / 2;
        desc.depth = 1;
        desc.mipCount = MipCount::FullChain;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::RG32_sFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;

        m_minMaxDepthPyramid = gRenderBackend.createImage(desc);
    }
    //sky shadow map
    {
        ImageDescription desc;
        desc.width = skyShadowMapRes;
        desc.height = skyShadowMapRes;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::Depth16;
        desc.usageFlags = ImageUsageFlags::Attachment | ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_skyShadowMap = gRenderBackend.createImage(desc);
    }
    //scene and history luminance
    {
        ImageDescription desc;
        desc.width = m_screenWidth;
        desc.height = m_screenHeight;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::R8;
        desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_sceneLuminance = gRenderBackend.createImage(desc);
        m_lastFrameLuminance = gRenderBackend.createImage(desc);
    }
    //noise textures
    for (int i = 0; i < noiseTextureCount; i++) {
        ImageDescription desc;
        desc.width = noiseTextureWidth;
        desc.height = noiseTextureHeight;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::RG8;
        desc.usageFlags = ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.autoCreateMips = false;

		const size_t channelCount = 2;
		desc.initialData = generateBlueNoiseTexture(glm::ivec2(noiseTextureWidth, noiseTextureHeight), channelCount);

        m_noiseTextures.push_back(gRenderBackend.createImage(desc));
    }
	//indirect diffuse Y component spherical harmonics
	{
		ImageDescription desc;
		desc.width = m_shadingConfig.indirectLightingHalfRes ? m_screenWidth / 2 : m_screenWidth;
		desc.height = m_shadingConfig.indirectLightingHalfRes ? m_screenHeight / 2 : m_screenHeight;
		desc.depth = 1;
		desc.type = ImageType::Type2D;
		desc.format = ImageFormat::RGBA16_sFloat;
		desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
		desc.mipCount = MipCount::One;
		desc.manualMipCount = 1;
		desc.autoCreateMips = false;

		m_indirectDiffuseHistory_Y_SH[0] = gRenderBackend.createImage(desc);
		m_indirectDiffuseHistory_Y_SH[1] = gRenderBackend.createImage(desc);

		m_indirectDiffuse_Y_SH[0] = gRenderBackend.createImage(desc);
		m_indirectDiffuse_Y_SH[1] = gRenderBackend.createImage(desc);
	}
	//indirect diffuse CoCg component
	{
		ImageDescription desc;
		desc.width = m_shadingConfig.indirectLightingHalfRes ? m_screenWidth / 2 : m_screenWidth;
		desc.height = m_shadingConfig.indirectLightingHalfRes ? m_screenHeight / 2 : m_screenHeight;
		desc.depth = 1;
		desc.type = ImageType::Type2D;
		desc.format = ImageFormat::RG16_sFloat;
		desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
		desc.mipCount = MipCount::One;
		desc.manualMipCount = 1;
		desc.autoCreateMips = false;

		m_indirectDiffuse_CoCg[0] = gRenderBackend.createImage(desc);
		m_indirectDiffuse_CoCg[1] = gRenderBackend.createImage(desc);
		m_indirectDiffuseHistory_CoCg[0] = gRenderBackend.createImage(desc);
		m_indirectDiffuseHistory_CoCg[1] = gRenderBackend.createImage(desc);
	}
	//world space normal buffer
	{
		ImageDescription desc;
		desc.width = m_screenWidth;
		desc.height = m_screenHeight;
		desc.depth = 1;
		desc.type = ImageType::Type2D;
		desc.format = ImageFormat::RGBA8;
		desc.usageFlags = ImageUsageFlags::Attachment | ImageUsageFlags::Sampled;
		desc.mipCount = MipCount::One;
		desc.manualMipCount = 1;
		desc.autoCreateMips = false;

		m_worldSpaceNormalImage = gRenderBackend.createImage(desc);
	}
	//half res depth
	{
		ImageDescription desc;
		desc.width = m_screenWidth / 2;
		desc.height = m_screenHeight / 2;
		desc.depth = 1;
		desc.type = ImageType::Type2D;
		desc.format = ImageFormat::R16_sFloat;
		desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
		desc.mipCount = MipCount::One;
		
		m_depthHalfRes = gRenderBackend.createImage(desc);
	}
	//indirect lighting full res Y component spherical harmonics
	{
		ImageDescription desc;
		desc.width = m_screenWidth;
		desc.height = m_screenHeight;
		desc.depth = 1;
		desc.type = ImageType::Type2D;
		desc.format = ImageFormat::RGBA16_sFloat;
		desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
		desc.mipCount = MipCount::One;
		desc.manualMipCount = 1;
		desc.autoCreateMips = false;

		m_indirectLightingFullRes_Y_SH = gRenderBackend.createImage(desc);
	}
	//indirect lighting full res CoCg component
	{
		ImageDescription desc;
		desc.width = m_screenWidth;
		desc.height = m_screenHeight;
		desc.depth = 1;
		desc.type = ImageType::Type2D;
		desc.format = ImageFormat::RG16_sFloat;
		desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
		desc.mipCount = MipCount::One;
		desc.manualMipCount = 1;
		desc.autoCreateMips = false;

		m_indirectLightingFullRes_CoCg = gRenderBackend.createImage(desc);
	}
}

void RenderFrontend::initSamplers(){

    //all samplers have maxMip set to 20, even when not using anisotropy
    //this ensures that shaders can use texelFetch and textureLod to access all mip levels

    //anisotropic wrap
    {
        SamplerDescription desc;
        desc.interpolation = SamplerInterpolation::Linear;
        desc.maxMip = 20;
        desc.useAnisotropy = true;
        desc.wrapping = SamplerWrapping::Repeat;
        desc.borderColor = SamplerBorderColor::White;

        m_sampler_anisotropicRepeat = gRenderBackend.createSampler(desc);
    }
    //nearest black border
    {
        SamplerDescription desc;
        desc.interpolation = SamplerInterpolation::Nearest;
        desc.wrapping = SamplerWrapping::Color;
        desc.useAnisotropy = false;
        desc.maxAnisotropy = 0;
        desc.borderColor = SamplerBorderColor::Black;
        desc.maxMip = 20;

        m_sampler_nearestBlackBorder = gRenderBackend.createSampler(desc);
    }
    //linear repeat
    {
        SamplerDescription desc;
        desc.interpolation = SamplerInterpolation::Linear;
        desc.wrapping = SamplerWrapping::Repeat;
        desc.useAnisotropy = false;
        desc.maxAnisotropy = 0;
        desc.borderColor = SamplerBorderColor::White;
        desc.maxMip = 20;

        m_sampler_linearRepeat = gRenderBackend.createSampler(desc);
    }
    //linear clamp
    {
        SamplerDescription desc;
        desc.interpolation = SamplerInterpolation::Linear;
        desc.wrapping = SamplerWrapping::Clamp;
        desc.useAnisotropy = false;
        desc.maxAnisotropy = 0;
        desc.borderColor = SamplerBorderColor::White;
        desc.maxMip = 20;

        m_sampler_linearClamp = gRenderBackend.createSampler(desc);
    }
    //nearest clamp
    {
        SamplerDescription desc;
        desc.interpolation = SamplerInterpolation::Nearest;
        desc.wrapping = SamplerWrapping::Clamp;
        desc.useAnisotropy = false;
        desc.maxMip = 20;
        desc.borderColor = SamplerBorderColor::Black;

        m_sampler_nearestClamp = gRenderBackend.createSampler(desc);
    }
    //linear White Border
    {
        SamplerDescription desc;
        desc.interpolation = SamplerInterpolation::Linear;
        desc.maxMip = 20;
        desc.useAnisotropy = false;
        desc.wrapping = SamplerWrapping::Color;
        desc.borderColor = SamplerBorderColor::White;

        m_sampler_linearWhiteBorder = gRenderBackend.createSampler(desc);
    }
	//nearest repeat
	{
		SamplerDescription desc;
		desc.interpolation = SamplerInterpolation::Nearest;
		desc.maxMip = 20;
		desc.useAnisotropy = false;
		desc.wrapping = SamplerWrapping::Repeat;
		desc.borderColor = SamplerBorderColor::Black;

		m_sampler_nearestRepeat = gRenderBackend.createSampler(desc);
	}
}

void RenderFrontend::initFramebuffers() {
    //shadow map framebuffers
    for (int i = 0; i < 4; i++) {
        FramebufferTarget depthTarget;
        depthTarget.image = m_shadowMaps[i];
        depthTarget.mipLevel = 0;

        FramebufferDescription desc;
        desc.compatibleRenderpass = m_shadowPasses[i];
        desc.targets = { depthTarget };
        m_shadowCascadeFramebuffers[i] = gRenderBackend.createFramebuffer(desc);
    }
}

void RenderFrontend::initRenderTargets() {
    for (int i = 0; i < 2; i++) {
        //motion buffer
		{
			ImageDescription desc;
			desc.width = m_screenWidth;
			desc.height = m_screenHeight;
			desc.depth = 1;
			desc.format = ImageFormat::RG16_sNorm;
			desc.autoCreateMips = false;
			desc.manualMipCount = 1;
			desc.mipCount = MipCount::One;
			desc.type = ImageType::Type2D;
			desc.usageFlags = ImageUsageFlags::Attachment | ImageUsageFlags::Sampled;

			m_frameRenderTargets[i].motionBuffer = gRenderBackend.createImage(desc);
		}
        //color buffer
        {
            ImageDescription desc;
            desc.initialData = std::vector<uint8_t>{};
            desc.width = m_screenWidth;
            desc.height = m_screenHeight;
            desc.depth = 1;
            desc.type = ImageType::Type2D;
            desc.format = ImageFormat::R11G11B10_uFloat;
            desc.usageFlags = ImageUsageFlags::Attachment | ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
            desc.mipCount = MipCount::One;
            desc.manualMipCount = 0;
            desc.autoCreateMips = false;

            m_frameRenderTargets[i].colorBuffer = gRenderBackend.createImage(desc);
        }
        //depth buffer
        {
            ImageDescription desc;
            desc.initialData = std::vector<uint8_t>{};
            desc.width = m_screenWidth;
            desc.height = m_screenHeight;
            desc.depth = 1;
            desc.type = ImageType::Type2D;
            desc.format = ImageFormat::Depth32;
            desc.usageFlags = ImageUsageFlags::Attachment | ImageUsageFlags::Sampled;
            desc.mipCount = MipCount::One;
            desc.manualMipCount = 0;
            desc.autoCreateMips = false;

            m_frameRenderTargets[i].depthBuffer = gRenderBackend.createImage(desc);
        }
        //color framebuffer
        {
            FramebufferTarget colorTarget;
            colorTarget.image = m_frameRenderTargets[i].colorBuffer;
            colorTarget.mipLevel = 0;

            FramebufferTarget depthTarget;
            depthTarget.image = m_frameRenderTargets[i].depthBuffer;
            depthTarget.mipLevel = 0;

            FramebufferDescription desc;
            desc.compatibleRenderpass = m_mainPass;
            desc.targets = { colorTarget, depthTarget };

            m_frameRenderTargets[i].colorFramebuffer = gRenderBackend.createFramebuffer(desc);
        }
        //prepass
        {
            FramebufferTarget motionTarget;
            motionTarget.image = m_frameRenderTargets[i].motionBuffer;
            motionTarget.mipLevel = 0;

			FramebufferTarget normalTarget;
			normalTarget.image = m_worldSpaceNormalImage;
			normalTarget.mipLevel = 0;

            FramebufferTarget depthTarget;
            depthTarget.image = m_frameRenderTargets[i].depthBuffer;
            depthTarget.mipLevel = 0;

            FramebufferDescription desc;
            desc.compatibleRenderpass = m_depthPrePass;
            desc.targets = { motionTarget, normalTarget, depthTarget };

            m_frameRenderTargets[i].prepassFramebuffer = gRenderBackend.createFramebuffer(desc);
        }
    }
}

void RenderFrontend::initBuffers(const HistogramSettings& histogramSettings) {
    //histogram buffer
    {
        StorageBufferDescription histogramBufferDesc;
        histogramBufferDesc.size = nHistogramBins * sizeof(uint32_t);
        m_histogramBuffer = gRenderBackend.createStorageBuffer(histogramBufferDesc);
    }
    //light buffer 
    {
        struct LightBuffer {
            glm::vec3 sunColor;
            float previousFrameExposure;
            float sunStrengthExposed;
        };

        LightBuffer initialData = {};
        StorageBufferDescription lightBufferDesc;
        lightBufferDesc.size = sizeof(LightBuffer);
        lightBufferDesc.initialData = &initialData;
        m_lightBuffer = gRenderBackend.createStorageBuffer(lightBufferDesc);
    }
    //per tile histogram
    {
        StorageBufferDescription histogramPerTileBufferDesc;
        histogramPerTileBufferDesc.size = (size_t)histogramSettings.maxTileCount * nHistogramBins * sizeof(uint32_t);
        m_histogramPerTileBuffer = gRenderBackend.createStorageBuffer(histogramPerTileBufferDesc);
    }
    //depth pyramid syncing buffer
    {
        StorageBufferDescription desc;
        desc.size = sizeof(uint32_t);
        desc.initialData = { (uint32_t)0 };
        m_depthPyramidSyncBuffer = gRenderBackend.createStorageBuffer(desc);
    }
    //light matrix buffer
    {
        StorageBufferDescription desc;
        const size_t splitSize = sizeof(glm::vec4);
        const size_t lightMatrixSize = sizeof(glm::mat4) * shadowCascadeCount;
        const size_t scaleInfoSize = sizeof(glm::vec2) * shadowCascadeCount;
        desc.size = splitSize + lightMatrixSize + scaleInfoSize;
        m_sunShadowInfoBuffer = gRenderBackend.createStorageBuffer(desc);
    }
    //sky atmosphere settings
    {
        UniformBufferDescription desc;
        desc.size = sizeof(AtmosphereSettings);
        m_atmosphereSettingsBuffer = gRenderBackend.createUniformBuffer(desc);
    }
    //global uniform buffer
    {
        UniformBufferDescription desc;
        desc.size = sizeof(m_globalShaderInfo);
        m_globalUniformBuffer = gRenderBackend.createUniformBuffer(desc);
    }
    //taa resolve weight buffer
    {
        UniformBufferDescription desc;
        desc.size = sizeof(float) * 9;
        m_taaResolveWeightBuffer = gRenderBackend.createUniformBuffer(desc);
    }
    //scene signed distance field volume info
    {
        UniformBufferDescription desc;
        desc.size = sizeof(VolumeInfo);
        m_sdfVolumeInfoBuffer = gRenderBackend.createUniformBuffer(desc);
    }
	//main pass transforms buffer
	{
		StorageBufferDescription desc;
		desc.size = sizeof(MainPassMatrices) * maxObjectCountMainScene;
		m_mainPassTransformsBuffer = gRenderBackend.createStorageBuffer(desc);
	}
	//shadow pass transforms buffer
	{
		StorageBufferDescription desc;
		desc.size = sizeof(glm::mat4) * maxObjectCountMainScene;
		m_shadowPassTransformsBuffer = gRenderBackend.createStorageBuffer(desc);
	}
	//bounding box debug rendering matrices
	{
		StorageBufferDescription desc;
		desc.size = maxObjectCountMainScene * sizeof(glm::mat4);
		m_boundingBoxDebugRenderMatrices = gRenderBackend.createStorageBuffer(desc);
	}
	//sdf instance buffer
	{
		StorageBufferDescription desc;
		desc.size = maxObjectCountMainScene * sizeof(SDFInstance) + sizeof(uint32_t) * 4; //extra uint for object count with padding to 16 byte
		m_sdfInstanceBuffer = gRenderBackend.createStorageBuffer(desc);
	}
	//sdf camera frustum culled instances
	{
		StorageBufferDescription desc;
		desc.size = maxObjectCountMainScene * sizeof(uint32_t) + sizeof(uint32_t);
		m_sdfCameraFrustumCulledInstances = gRenderBackend.createStorageBuffer(desc);
	}
	//sdf shadow frustum culled instances
	{
		StorageBufferDescription desc;
		desc.size = maxObjectCountMainScene * sizeof(uint32_t) + sizeof(uint32_t);
		m_sdfShadowFrustumCulledInstances = gRenderBackend.createStorageBuffer(desc);
	}
	//camera frustum buffer
	{
		UniformBufferDescription desc;
		desc.size = 12 * sizeof(glm::vec4);
		m_cameraFrustumBuffer = gRenderBackend.createUniformBuffer(desc);
	}
	//shadow frustum buffer
	{
		UniformBufferDescription desc;
		desc.size = 10 * sizeof(glm::vec4);
		m_shadowFrustumPointsBuffer = gRenderBackend.createUniformBuffer(desc);
	}
	//sdf instance world bounding boxes
	{
		StorageBufferDescription desc;
		desc.size = maxObjectCountMainScene * 2 * sizeof(glm::vec4);
		m_sdfInstanceWorldBBBuffer = gRenderBackend.createStorageBuffer(desc);
	}
	//sdf camera culled tiles
	{
		StorageBufferDescription desc;
		//FIXME: handle bigger resolutions and don't allocate for worst case
		const size_t maxWidth = 1920;
		const size_t maxHeight = 1080;
		const size_t tileCount = glm::ceil(maxWidth / sdfCameraCullingTileSize) * glm::ceil(maxHeight / sdfCameraCullingTileSize);
		const size_t tileSize = maxSdfObjectsPerTile * sizeof(uint32_t) + sizeof(uint32_t); //one uint index per object + object count
		desc.size = tileCount * tileSize;
		m_sdfCameraCulledTiles = gRenderBackend.createStorageBuffer(desc);
	}
	//sdf shadow culled tiles
	{
		StorageBufferDescription desc;
		const size_t tileCount = sdfShadowCullingTilesCount1D * sdfShadowCullingTilesCount1D;
		const size_t tileSize = tileCount * sizeof(uint32_t) + sizeof(uint32_t); //one uint index per object + object count
		desc.size = tileCount * tileSize;
		m_sdfShadowCulledTiles = gRenderBackend.createStorageBuffer(desc);
	}
	//shadow frustum info buffer
	{
		UniformBufferDescription desc;
		desc.size = sizeof(ShadowFrustumInfo);
		m_shadowFrustumInfoBuffer = gRenderBackend.createUniformBuffer(desc);
	}
	//sdf trace influence range buffer
	{
		UniformBufferDescription desc;
		desc.size = sizeof(float);
		m_sdfTraceInfluenceRangeBuffer = gRenderBackend.createUniformBuffer(desc);
	}
}

void RenderFrontend::initMeshs() {
    //dynamic meshes for frustum debugging
    {
        m_cameraFrustumModel = gRenderBackend.createDynamicMeshes(
            { positionsInViewFrustumLineMesh }, { indicesInViewFrustumLineMesh }).front();

        m_shadowFrustumModel = gRenderBackend.createDynamicMeshes(
            { positionsInViewFrustumLineMesh }, { indicesInViewFrustumLineMesh }).front();
    }
    //skybox cube
    {
        MeshData cubeData;
        cubeData.positions = {
            glm::vec3(-1.f, -1.f, -1.f),
            glm::vec3(1.f, -1.f, -1.f),
            glm::vec3(1.f, 1.f, -1.f),
            glm::vec3(-1.f, 1.f, -1.f),
            glm::vec3(-1.f, -1.f, 1.f),
            glm::vec3(1.f, -1.f, 1.f),
            glm::vec3(1.f, 1.f, 1.f),
            glm::vec3(-1.f, 1.f, 1.f)
        };
        cubeData.uvs = {
            glm::vec2(),
            glm::vec2(),
            glm::vec2(),
            glm::vec2(),
            glm::vec2(),
            glm::vec2(),
            glm::vec2(),
            glm::vec2()
        };
        cubeData.normals = {
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3()
        };
        cubeData.tangents = {
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3()
        };
        cubeData.bitangents = {
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3()
        };
        cubeData.indices = {
            0, 1, 3, 3, 1, 2,
            1, 5, 2, 2, 5, 6,
            5, 4, 6, 6, 4, 7,
            4, 0, 7, 7, 0, 3,
            3, 2, 7, 7, 2, 6,
            4, 5, 0, 0, 5, 1
        };
        const std::vector<MeshBinary> cubeBinary = meshesToBinary(std::vector<MeshData>{cubeData}, AABBListFromMeshes({cubeData}));

        m_skyCube = gRenderBackend.createMeshes(cubeBinary).back();
    }
    //quad 
    {
        MeshData quadData;
        quadData.positions = {
            glm::vec3(-1.f, -1.f, -1.f),
            glm::vec3(1.f, 1.f, -1.f),
            glm::vec3(1.f, -1.f, -1.f),
            glm::vec3(-1.f, 1.f, -1.f)
        };
        quadData.uvs = {
            glm::vec2(),
            glm::vec2(),
            glm::vec2(),
            glm::vec2()
        };
        quadData.normals = {
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3()
        };
        quadData.tangents = {
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3()
        };
        quadData.bitangents = {
            glm::vec3(),
            glm::vec3(),
            glm::vec3(),
            glm::vec3()
        };
        quadData.indices = {
            0, 1, 2, 
            0, 1, 3
        };
        const std::vector<MeshBinary> quadBinary = meshesToBinary(std::vector<MeshData>{quadData}, AABBListFromMeshes({ quadData }));

        m_quad = gRenderBackend.createMeshes(quadBinary).back();
    }
	//bounding box mesh
	//drawn using lines, so needs different positions than skybox
	{
		AxisAlignedBoundingBox normalizedBB;
		normalizedBB.min = glm::vec3(-1);
		normalizedBB.max = glm::vec3(1);

		std::vector<glm::vec3> positions;
		std::vector<uint32_t> indices;
		axisAlignedBoundingBoxToLineMesh(normalizedBB, &positions, &indices);

		MeshBinary binary;
		binary.boundingBox = normalizedBB;
		binary.indexCount = indices.size();
		binary.vertexCount = positions.size();
		binary.vertexBuffer.resize(sizeof(glm::vec3) * positions.size());
		memcpy(binary.vertexBuffer.data(), positions.data(), binary.vertexBuffer.size());
		for (const uint32_t& index : indices) {
			binary.indexBuffer.push_back(index);	//conversion to 16 bit index
		}

		m_boundingBoxMesh = gRenderBackend.createMeshes({ binary }).back();
	}
}

void RenderFrontend::initRenderpasses(const HistogramSettings& histogramSettings) {
    //main shading pass
    {
        const Attachment colorAttachment(ImageFormat::R11G11B10_uFloat, AttachmentLoadOp::Clear);
        const Attachment depthAttachment(ImageFormat::Depth32, AttachmentLoadOp::Load);

        GraphicPassDescription mainPassDesc;
        mainPassDesc.name = "Forward shading";
        mainPassDesc.shaderDescriptions = createForwardPassShaderDescription(m_shadingConfig);
        mainPassDesc.attachments = { colorAttachment, depthAttachment };
        mainPassDesc.depthTest.function = DepthFunction::Equal;
        mainPassDesc.depthTest.write = true;
        mainPassDesc.rasterization.cullMode = CullMode::Back;
        mainPassDesc.rasterization.mode = RasterizationeMode::Fill;
        mainPassDesc.blending = BlendState::None;
        mainPassDesc.vertexFormat = VertexFormat::Full;

        m_mainPass = gRenderBackend.createGraphicPass(mainPassDesc);
    }
    //shadow cascade passes
    for (uint32_t cascade = 0; cascade < shadowCascadeCount; cascade++) {

        const Attachment shadowMapAttachment(ImageFormat::Depth16, AttachmentLoadOp::Clear);

        GraphicPassDescription shadowPassConfig;
        shadowPassConfig.name = "Shadow map cascade " + std::to_string(cascade);
        shadowPassConfig.attachments = { shadowMapAttachment };
        shadowPassConfig.shaderDescriptions.vertex.srcPathRelative = "sunShadow.vert";
        shadowPassConfig.shaderDescriptions.fragment.srcPathRelative = "sunShadow.frag";
        shadowPassConfig.depthTest.function = DepthFunction::GreaterEqual;
        shadowPassConfig.depthTest.write = true;
        shadowPassConfig.rasterization.cullMode = CullMode::Front;
        shadowPassConfig.rasterization.mode = RasterizationeMode::Fill;
        shadowPassConfig.rasterization.clampDepth = true;
        shadowPassConfig.blending = BlendState::None;
        shadowPassConfig.vertexFormat = VertexFormat::Full;

        //cascade index specialisation constant
        shadowPassConfig.shaderDescriptions.vertex.specialisationConstants = { {
            0,                                                  //location
            dataToCharArray((void*)&cascade, sizeof(cascade))   //value
            }};
        

        const auto shadowPass = gRenderBackend.createGraphicPass(shadowPassConfig);
        m_shadowPasses.push_back(shadowPass);
    }
    //sky copy pass
    {
        ComputePassDescription cubeWriteDesc;
        cubeWriteDesc.name = "Copy sky to cubemap";
        cubeWriteDesc.shaderDescription.srcPathRelative = "copyToCube.comp";
        m_toCubemapPass = gRenderBackend.createComputePass(cubeWriteDesc);
    }
    //cubemap mip creation pass
    {
        ComputePassDescription cubemapMipPassDesc;
        cubemapMipPassDesc.name = "Sky mip creation";
        cubemapMipPassDesc.shaderDescription.srcPathRelative = "cubemapMip.comp";

        //first map is written to by different shader        
        for (uint32_t i = 0; i < skyTextureMipCount - 1; i++) {
            m_cubemapMipPasses.push_back(gRenderBackend.createComputePass(cubemapMipPassDesc));
        }
    }
    //specular convolution pass
    {
        for (uint32_t i = 0; i < m_specularSkyProbeMipCount; i++) {
            ComputePassDescription specularConvolutionDesc;
            specularConvolutionDesc.name = "Specular sky probe convolution";
            specularConvolutionDesc.shaderDescription.srcPathRelative = "specularSkyConvolution.comp";

            //specialisation constants
            {
                auto& constants = specularConvolutionDesc.shaderDescription.specialisationConstants;

                //mip count specialisation constant
                constants.push_back({
                    0,                                                                                  //location
                    dataToCharArray((void*)&m_specularSkyProbeMipCount, sizeof(m_specularSkyProbeMipCount))   //value
                    });
                //mip level
                constants.push_back({
                    1,                                      //location
                    dataToCharArray((void*)&i, sizeof(i))   //value
                    });
            }
            m_skySpecularConvolutionPerMipPasses.push_back(gRenderBackend.createComputePass(specularConvolutionDesc));
        }
    }
    //diffuse convolution pass
    {
        ComputePassDescription diffuseConvolutionDesc;
        diffuseConvolutionDesc.name = "Diffuse sky probe convolution";
        diffuseConvolutionDesc.shaderDescription.srcPathRelative = "diffuseSkyConvolution.comp";
        m_skyDiffuseConvolutionPass = gRenderBackend.createComputePass(diffuseConvolutionDesc);
    }
    //sky transmission lut creation pass
    {
        ComputePassDescription skyTransmissionLutPassDesc;
        skyTransmissionLutPassDesc.name = "Sky transmission lut";
        skyTransmissionLutPassDesc.shaderDescription.srcPathRelative = "skyTransmissionLut.comp";
        m_skyTransmissionLutPass = gRenderBackend.createComputePass(skyTransmissionLutPassDesc);
    }
    //sky multiscatter lut
    {
        ComputePassDescription skyMultiscatterPassDesc;
        skyMultiscatterPassDesc.name = "Sky multiscatter lut";
        skyMultiscatterPassDesc.shaderDescription.srcPathRelative = "skyMultiscatterLut.comp";
        m_skyMultiscatterLutPass = gRenderBackend.createComputePass(skyMultiscatterPassDesc);
    }
    //sky lut creation pass
    {
        ComputePassDescription skyLutPassDesc;
        skyLutPassDesc.name = "Sky lut";
        skyLutPassDesc.shaderDescription.srcPathRelative = "skyLut.comp";
        m_skyLutPass = gRenderBackend.createComputePass(skyLutPassDesc);
    }
    //sky pass
    {
        const Attachment colorAttachment(ImageFormat::R11G11B10_uFloat, AttachmentLoadOp::Load);
        const Attachment depthAttachment(ImageFormat::Depth32, AttachmentLoadOp::Load);

        GraphicPassDescription skyPassConfig;
        skyPassConfig.name = "Skybox render";
        skyPassConfig.attachments = { colorAttachment, depthAttachment };
        skyPassConfig.shaderDescriptions.vertex.srcPathRelative = "sky.vert";
        skyPassConfig.shaderDescriptions.fragment.srcPathRelative = "sky.frag";
        skyPassConfig.depthTest.function = DepthFunction::GreaterEqual;
        skyPassConfig.depthTest.write = false;
        skyPassConfig.rasterization.cullMode = CullMode::None;
        skyPassConfig.rasterization.mode = RasterizationeMode::Fill;
        skyPassConfig.blending = BlendState::None;
        skyPassConfig.vertexFormat = VertexFormat::Full;

        m_skyPass = gRenderBackend.createGraphicPass(skyPassConfig);
    }
    //sun sprite
    {
        const auto colorAttachment = Attachment(ImageFormat::R11G11B10_uFloat, AttachmentLoadOp::Load);
        const auto depthAttachment = Attachment(ImageFormat::Depth32, AttachmentLoadOp::Load);

        GraphicPassDescription desc;
        desc.name = "Sun sprite";
        desc.attachments = { colorAttachment, depthAttachment };
        desc.shaderDescriptions.vertex.srcPathRelative = "sunSprite.vert";
        desc.shaderDescriptions.fragment.srcPathRelative = "sunSprite.frag";
        desc.depthTest.function = DepthFunction::GreaterEqual;
        desc.depthTest.write = false;
        desc.rasterization.cullMode = CullMode::None;
        desc.rasterization.mode = RasterizationeMode::Fill;
        desc.blending = BlendState::Additive;
        desc.vertexFormat = VertexFormat::Full;

        m_sunSpritePass = gRenderBackend.createGraphicPass(desc);
    }
    //BRDF Lut creation pass
    {
        ComputePassDescription brdfLutPassDesc;
        brdfLutPassDesc.name = "BRDF Lut creation";
        brdfLutPassDesc.shaderDescription = createBRDFLutShaderDescription(m_shadingConfig);
        m_brdfLutPass = gRenderBackend.createComputePass(brdfLutPassDesc);
    }
    //geometry debug pass
    {
        const auto colorAttachment = Attachment(ImageFormat::R11G11B10_uFloat, AttachmentLoadOp::Load);
        const auto depthAttachment = Attachment(ImageFormat::Depth32, AttachmentLoadOp::Load);

        GraphicPassDescription debugPassConfig;
        debugPassConfig.name = "Debug geometry";
        debugPassConfig.attachments = { colorAttachment, depthAttachment };
        debugPassConfig.shaderDescriptions.vertex.srcPathRelative = "debug.vert";
        debugPassConfig.shaderDescriptions.fragment.srcPathRelative = "debug.frag";
        debugPassConfig.depthTest.function = DepthFunction::GreaterEqual;
        debugPassConfig.depthTest.write = true;
        debugPassConfig.rasterization.cullMode = CullMode::None;
        debugPassConfig.rasterization.mode = RasterizationeMode::Line;
        debugPassConfig.blending = BlendState::None;
        debugPassConfig.vertexFormat = VertexFormat::PositionOnly;

        m_debugGeoPass = gRenderBackend.createGraphicPass(debugPassConfig);
    }
    //histogram per tile pass
    {
        ComputePassDescription histogramPerTileDesc;
        histogramPerTileDesc.name = "Histogram per tile";
        histogramPerTileDesc.shaderDescription.srcPathRelative = "histogramPerTile.comp";

        const uint32_t maxTilesSpecialisationConstantID = 4;

        //specialisation constants
        {
            auto& constants = histogramPerTileDesc.shaderDescription.specialisationConstants;

            //bin count
            constants.push_back({
                0,                                                                  //location
                dataToCharArray((void*)&nHistogramBins, sizeof(nHistogramBins)) //value
                });
            //min luminance constant
            constants.push_back({
                1,                                                                                      //location
                dataToCharArray((void*)&histogramSettings.minValue, sizeof(histogramSettings.minValue)) //value
                });
            //max luminance constant
            constants.push_back({
                2,                                                                                      //location
                dataToCharArray((void*)&histogramSettings.maxValue, sizeof(histogramSettings.maxValue)) //value
                });
            constants.push_back({
                3,                                                                                              //location
                dataToCharArray((void*)&histogramSettings.maxTileCount, sizeof(histogramSettings.maxTileCount)) //value
                });
        }
        m_histogramPerTilePass = gRenderBackend.createComputePass(histogramPerTileDesc);
    }
    //histogram reset pass
    {
        ComputePassDescription resetDesc;
        resetDesc.name = "Histogram reset";
        resetDesc.shaderDescription.srcPathRelative = "histogramReset.comp";

        //bin count constant
        resetDesc.shaderDescription.specialisationConstants.push_back({
            0,                                                                  //location
            dataToCharArray((void*)&nHistogramBins, sizeof(nHistogramBins)) //value
            });

        m_histogramResetPass = gRenderBackend.createComputePass(resetDesc);
    }
    //histogram combine tiles pass
    {
        const uint32_t maxTilesSpecialisationConstantID = 1;

        ComputePassDescription histogramCombineDesc;
        histogramCombineDesc.name = "Histogram combine tiles";
        histogramCombineDesc.shaderDescription.srcPathRelative = "histogramCombineTiles.comp";

        auto& constants = histogramCombineDesc.shaderDescription.specialisationConstants;

        //bin count
        constants.push_back({
            0,                                                                  //location
            dataToCharArray((void*)&nHistogramBins, sizeof(nHistogramBins)) //value
                });
        //max luminance constant
        constants.push_back({
            1,                                                                                              //location
            dataToCharArray((void*)&histogramSettings.maxTileCount, sizeof(histogramSettings.maxTileCount)) //value
                });

        m_histogramCombinePass = gRenderBackend.createComputePass(histogramCombineDesc);
    }
    //pre-expose lights pass
    {
        ComputePassDescription preExposeLightsDesc;
        preExposeLightsDesc.name = "Pre-expose lights";
        preExposeLightsDesc.shaderDescription.srcPathRelative = "preExposeLights.comp";

        //specialisation constants
        {
            auto& constants = preExposeLightsDesc.shaderDescription.specialisationConstants;

            //bin count
            constants.push_back({
                0,                                                              //location
                dataToCharArray((void*)&nHistogramBins, sizeof(nHistogramBins)) //value
                });
            //min luminance constant
            constants.push_back({
                1,                                                                                      //location
                dataToCharArray((void*)&histogramSettings.minValue,sizeof(histogramSettings.minValue))  //value
                });
            //max luminance constant
            constants.push_back({
                2,                                                                                      //location
                dataToCharArray((void*)&histogramSettings.maxValue, sizeof(histogramSettings.maxValue)) //value
                });
        }
        m_preExposeLightsPass = gRenderBackend.createComputePass(preExposeLightsDesc);
    }
    //depth prepass
    {
        Attachment depthAttachment(ImageFormat::Depth32, AttachmentLoadOp::Clear);
        Attachment velocityAttachment(ImageFormat::RG16_sNorm, AttachmentLoadOp::Clear);
		Attachment normalAttachment(ImageFormat::RGBA8, AttachmentLoadOp::Clear);

        GraphicPassDescription desc;
        desc.attachments = { velocityAttachment, normalAttachment, depthAttachment };
        desc.blending = BlendState::None;
        desc.depthTest.function = DepthFunction::GreaterEqual;
        desc.depthTest.write = true;
        desc.name = "Depth prepass";
        desc.rasterization.cullMode = CullMode::Back;
        desc.shaderDescriptions.vertex.srcPathRelative = "depthPrepass.vert";
        desc.shaderDescriptions.fragment.srcPathRelative = "depthPrepass.frag";
        desc.vertexFormat = VertexFormat::Full;

        m_depthPrePass = gRenderBackend.createGraphicPass(desc);
    }
    //depth pyramid pass
    {
        ComputePassDescription desc;
        desc.name = "Depth min/max pyramid creation";

        uint32_t threadgroupCount = 0;
        desc.shaderDescription = createDepthPyramidShaderDescription(&threadgroupCount);

        m_depthPyramidPass = gRenderBackend.createComputePass(desc);
    }
    //light matrix pass
    {
        ComputePassDescription desc;
        desc.name = "Compute light matrix";
        desc.shaderDescription.srcPathRelative = "lightMatrix.comp";

        m_lightMatrixPass = gRenderBackend.createComputePass(desc);
    }
    //tonemapping pass
    {
        ComputePassDescription desc;
        desc.name = "Tonemapping";
        desc.shaderDescription.srcPathRelative = "tonemapping.comp";

        m_tonemappingPass = gRenderBackend.createComputePass(desc);
    }
    //temporal filter pass
    {
        ComputePassDescription desc;
        desc.name = "Temporal filtering";
        desc.shaderDescription = createTemporalFilterShaderDescription();
        m_temporalFilterPass = gRenderBackend.createComputePass(desc);
    }
    //temporal supersampling pass
    {
        ComputePassDescription desc;
        desc.name = "Temporal supersampling";
        desc.shaderDescription = createTemporalSupersamplingShaderDescription();
        m_temporalSupersamplingPass = gRenderBackend.createComputePass(desc);
    }
    //hdr image copy pass
    {
        ComputePassDescription desc;
        desc.name = "Image copy";
        desc.shaderDescription.srcPathRelative = "imageCopyHDR.comp";
        m_hdrImageCopyPass = gRenderBackend.createComputePass(desc);
    }
    //color to luminance pass
    {
        ComputePassDescription desc;
        desc.name = "Color to Luminance";
        desc.shaderDescription.srcPathRelative = "colorToLuminance.comp";
        m_colorToLuminancePass = gRenderBackend.createComputePass(desc);
    }
    //sdf debug pass
    {
        ComputePassDescription desc;
        desc.name = "SDF Debug";
        desc.shaderDescription.srcPathRelative = "SDFDebug.comp";
        m_sdfDebugPass = gRenderBackend.createComputePass(desc);
    }
	//sdf indirect lighting
	{
		ComputePassDescription desc;
		desc.name = "Indirect SDF lighting";
		desc.shaderDescription.srcPathRelative = "sdfDiffuseTrace.comp";
		m_diffuseSDFTracePass = gRenderBackend.createComputePass(desc);
	}
	//indirect diffuse spatial filter
	{
		for (int i = 0; i < 2; i++) {
			ComputePassDescription desc;
			desc.name = "Indirect diffuse spatial filter";
			desc.shaderDescription.srcPathRelative = "filterIndirectDiffuseSpatial.comp";

			//index constant
			desc.shaderDescription.specialisationConstants.push_back({
			0,										//location
			dataToCharArray((void*)&i, sizeof(i))   //value
			});
			m_indirectDiffuseFilterSpatialPass[i] = gRenderBackend.createComputePass(desc);
		}
	}
	//indirect diffuse temporal filter
	{
		ComputePassDescription desc;
		desc.name = "Indirect diffuse temporal filter";
		desc.shaderDescription.srcPathRelative = "filterIndirectDiffuseTemporal.comp";
		m_indirectDiffuseFilterTemporalPass = gRenderBackend.createComputePass(desc);
	}
	//depth downscale
	{
		ComputePassDescription desc;
		desc.name = "Depth downscale";
		desc.shaderDescription.srcPathRelative = "depthDownscale.comp";
		m_depthDownscalePass = gRenderBackend.createComputePass(desc);
	}
	//indirect lighting upscale
	{
		ComputePassDescription desc;
		desc.name = "Indirect lighting upscale";
		desc.shaderDescription.srcPathRelative = "indirectLightUpscale.comp";
		m_indirectLightingUpscale = gRenderBackend.createComputePass(desc);
	}
	//sdf instance frustum culling
	{
		ComputePassDescription desc;
		desc.name = "SDF camera frustum culling";
		desc.shaderDescription.srcPathRelative = "sdfCameraFrustumCulling.comp";
		m_sdfCameraFrustumCulling = gRenderBackend.createComputePass(desc);
	}
	//sdf shadow frustum culling
	{
		ComputePassDescription desc;
		desc.name = "SDF shadow frustum culling";
		desc.shaderDescription.srcPathRelative = "sdfShadowFrustumCulling.comp";
		m_sdfShadowFrustumCulling = gRenderBackend.createComputePass(desc);
	}
	//sdf camera tile culling
	{
		ComputePassDescription desc;
		desc.name = "SDF camera tile culling";
		desc.shaderDescription.srcPathRelative = "sdfCameraTileCulling.comp";
		m_sdfCameraTileCulling = gRenderBackend.createComputePass(desc);
	}
	//sdf shadow tile culling
	{
		ComputePassDescription desc;
		desc.name = "SDF shadow tile culling";
		desc.shaderDescription.srcPathRelative = "sdfShadowTileCulling.comp";
		m_sdfShadowTileCulling = gRenderBackend.createComputePass(desc);
	}
}

ShaderDescription RenderFrontend::createDepthPyramidShaderDescription(uint32_t* outThreadgroupCount) {

    ShaderDescription desc;
    desc.srcPathRelative = "depthHiZPyramid.comp";

    const uint32_t width = m_screenWidth / 2;
    const uint32_t height = m_screenHeight / 2;
    const uint32_t depthMipCount = mipCountFromResolution(width, height, 1);

    const uint32_t maxMipCount = 11; //see shader for details
    const auto dispatchCount = computeSinglePassMipChainDispatchCount(width, height, depthMipCount, maxMipCount);

    //mip count
    desc.specialisationConstants.push_back({ 
        0,                                                              //location
        dataToCharArray((void*)&depthMipCount, sizeof(depthMipCount))   //value
            });
    //depth buffer width
    desc.specialisationConstants.push_back({
        1,                                                              //location
        dataToCharArray((void*)&m_screenWidth, sizeof(m_screenWidth))   //value
            });
    //depth buffer height
    desc.specialisationConstants.push_back({
        2,                                                              //location
        dataToCharArray((void*)&m_screenHeight, sizeof(m_screenHeight)) //value
            });
    //threadgroup count
    *outThreadgroupCount = dispatchCount.x * dispatchCount.y;
    desc.specialisationConstants.push_back({
        3,                                                                          //location
        dataToCharArray((void*)outThreadgroupCount, sizeof(*outThreadgroupCount))   //value
            });

    return desc;
}

glm::ivec2 RenderFrontend::computeSinglePassMipChainDispatchCount(const uint32_t width, const uint32_t height, const uint32_t mipCount, const uint32_t maxMipCount) const {

    //shader can process up to 12 mip levels
    //thread group extent ranges from 16 to 1 depending on how many mips are used
    const uint32_t unusedMips = maxMipCount - mipCount;

    //last 6 mips are processed by single thread group
    if (unusedMips >= 6) {
        return glm::ivec2(1, 1);
    }
    else {
        //group size of 16x16 can compute up to a 32x32 area in mip0
        const uint32_t localThreadGroupExtent = 32 / (uint32_t)pow((uint32_t)2, unusedMips);

        glm::ivec2 count;
        count.x = (uint32_t)std::ceil(float(width) / localThreadGroupExtent);
        count.y = (uint32_t)std::ceil(float(height) / localThreadGroupExtent);

        return count;
    }
}

void RenderFrontend::drawUi() {
    //rendering stats
    {
        ImGui::Begin("Rendering stats");
        ImGui::Text(("DeltaTime: " + std::to_string(m_globalShaderInfo.deltaTime * 1000) + "ms").c_str());
        ImGui::Text(("Mesh count: " + std::to_string(m_currentMeshCount)).c_str());
        ImGui::Text(("Main pass drawcalls: " + std::to_string(m_currentMainPassDrawcallCount)).c_str());
        ImGui::Text(("Shadow map drawcalls: " + std::to_string(m_currentShadowPassDrawcallCount)).c_str());

        uint64_t allocatedMemorySizeByte;
        uint64_t usedMemorySizeByte;
        gRenderBackend.getMemoryStats(&allocatedMemorySizeByte, &usedMemorySizeByte);

        const float byteToMbDivider = 1048576;
        const float allocatedMemorySizeMegaByte = allocatedMemorySizeByte / byteToMbDivider;
        const float usedMemorySizeMegaByte = usedMemorySizeByte / byteToMbDivider;

        ImGui::Text(("Allocated memory: " + std::to_string(allocatedMemorySizeMegaByte) + "mb").c_str());
        ImGui::Text(("Used memory: " + std::to_string(usedMemorySizeMegaByte) + "mb").c_str());
    }

    //pass timings shown in columns
    {
        m_renderTimingTimeSinceLastUpdate += m_globalShaderInfo.deltaTime;
        if (m_renderTimingTimeSinceLastUpdate > m_renderTimingUpdateFrequency) {
            m_currentRenderTimings = gRenderBackend.getRenderpassTimings();
            m_renderTimingTimeSinceLastUpdate = 0.f;
        }
        
        ImGui::Separator();
        ImGui::Columns(2);
        for (const auto timing : m_currentRenderTimings) {
            ImGui::Text(timing.name.c_str());
        }
        ImGui::NextColumn();
        for (const auto timing : m_currentRenderTimings) {
            //limit number of decimal places to improve readability
            const size_t commaIndex = std::max(int(timing.timeMs) / 10, 1);
            const size_t decimalPlacesToKeep = 2;
            auto timeString = std::to_string(timing.timeMs);
            timeString = timeString.substr(0, commaIndex + 1 + decimalPlacesToKeep);
            ImGui::Text(timeString.c_str());
        }
    }
    ImGui::End();

    ImGui::Begin("Rendering");

	if (ImGui::CollapsingHeader("SDF settings")) {
		const char* sdfDebugOptions[] = { "None", "Visualize SDF" };
		ImGui::Combo("SDF debug mode", (int*)&m_sdfDebugMode, sdfDebugOptions, 2);
		ImGui::InputFloat("Shadow distance", &m_sdfShadowDistance);
		ImGui::InputFloat("SDF trace influence range", &m_sdfTraceInfluenceRadius);
	}

    //Temporal filter Settings
    if(ImGui::CollapsingHeader("Temporal filter settings")){

        if (ImGui::Checkbox("Enabled", &m_temporalFilterSettings.enabled)) {
            m_globalShaderInfo.cameraCut = true;
        }

        ImGui::Checkbox("Separate temporal supersampling", &m_temporalFilterSettings.useSeparateSupersampling);

        m_isTemporalFilterShaderDescriptionStale |= ImGui::Checkbox("Clipping", &m_temporalFilterSettings.useClipping);
        m_isTemporalFilterShaderDescriptionStale |= ImGui::Checkbox("Dilate motion vector", &m_temporalFilterSettings.useMotionVectorDilation);

        const char* historySamplingOptions[] = { "Bilinear", "Bicubic16Tap", "Bicubic9Tap", "Bicubic5Tap", "Bicubic1Tap" };
        m_isTemporalFilterShaderDescriptionStale |= ImGui::Combo("Bicubic history sample", 
            (int*)&m_temporalFilterSettings.historySamplingTech, historySamplingOptions, 5);

        m_isTemporalSupersamplingShaderDescriptionStale |= ImGui::Checkbox("Tonemap temporal filter input", &m_temporalFilterSettings.supersampleUseTonemapping);
        m_isTemporalFilterShaderDescriptionStale |= ImGui::Checkbox("Tonemap temporal supersample input", &m_temporalFilterSettings.filterUseTonemapping);
        ImGui::Checkbox("Use mip bias", &m_temporalFilterSettings.useMipBias);
    }
    //lighting settings
    if(ImGui::CollapsingHeader("Lighting settings")){
        ImGui::DragFloat2("Sun direction", &m_sunDirection.x);
        ImGui::DragFloat("Exposure offset EV", &m_globalShaderInfo.exposureOffset, 0.1f);
        ImGui::DragFloat("Adaption speed EV/s", &m_globalShaderInfo.exposureAdaptionSpeedEvPerSec, 0.1f, 0.f);
        ImGui::InputFloat("Sun Illuminance Lux", &m_globalShaderInfo.sunIlluminanceLux);
    }
    //shading settings
    if (ImGui::CollapsingHeader("Shading settings")) {
        m_isMainPassShaderDescriptionStale |= ImGui::Checkbox("Indirect Multiscatter BRDF", &m_shadingConfig.useIndirectMultiscatter);

        //naming and values rely on enum values being ordered same as names and indices being [0,3]
        const char* diffuseBRDFOptions[] = { "Lambert", "Disney", "CoD WWII", "Titanfall 2" };
        const bool diffuseBRDFChanged = ImGui::Combo("Diffuse BRDF",
            (int*)&m_shadingConfig.diffuseBRDF,
            diffuseBRDFOptions, 4);
        m_isMainPassShaderDescriptionStale |= diffuseBRDFChanged;
        m_isBRDFLutShaderDescriptionStale = diffuseBRDFChanged;

        //naming and values rely on enum values being ordered same as names and indices being [0,3]
        const char* directMultiscatterBRDFOptions[] = { "McAuley", "Simplified", "Scaled GGX lobe", "None" };
        m_isMainPassShaderDescriptionStale |= ImGui::Combo("Direct Multiscatter BRDF",
            (int*)&m_shadingConfig.directMultiscatter,
            directMultiscatterBRDFOptions, 4);

		const char* indirectLightingLabels[] = { "SDF trace", "Constant ambient" };
		m_isMainPassShaderDescriptionStale |= ImGui::Combo("Indirect lighting tech", (int*)&m_shadingConfig.indirectLightingTech,
			indirectLightingLabels, 2);

        m_isMainPassShaderDescriptionStale |= ImGui::Checkbox("Geometric AA", &m_shadingConfig.useGeometryAA);

		if (ImGui::Checkbox("Indirect lighting half resolution", &m_shadingConfig.indirectLightingHalfRes)) {
			resizeIndirectLightingBuffers();
			m_globalShaderInfo.cameraCut = true;
		}
    }
    //camera settings
    if (ImGui::CollapsingHeader("Camera settings")) {
        ImGui::InputFloat("Near plane", &m_camera.intrinsic.near);
        ImGui::InputFloat("Far plane", &m_camera.intrinsic.far);
    }
	if (ImGui::CollapsingHeader("Debug settings")) {
		ImGui::Checkbox("Render bounding boxes", &m_renderBoundingBoxes);
	}
    ImGui::End();
}