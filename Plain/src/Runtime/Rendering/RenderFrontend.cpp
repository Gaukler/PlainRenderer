#include "pch.h"
#include "RenderFrontend.h"

// disable ImGui warning
#pragma warning( push )
#pragma warning( disable : 26495)

#include <imgui/imgui.h>

// reenable warning
#pragma warning( pop )

#include <Utilities/MathUtils.h>
#include "Runtime/Timer.h"
#include "Culling.h"
#include "Utilities/GeneralUtils.h"
#include "Common/MeshProcessing.h"
#include "Noise.h"
#include "Common/Utilities/DirectoryUtils.h"
#include "Common/ImageIO.h"
#include "Common/sdfUtilities.h"
#include "Common/JobSystem.h"
#include "Runtime/Rendering/SceneConfig.h"
#include "Common/VolumeInfo.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

//---- private function declarations ----

void resizeCallback(GLFWwindow* window, int width, int height);
DefaultTextures createDefaultTextures();
glm::ivec3 computeVolumetricLightingFroxelResolution(const uint32_t screenResolutionX, const uint32_t screenResolutionY);

//---- private constants ----

// definition of extern variable from header
RenderFrontend gRenderFrontend;

const uint32_t shadowMapRes = 2048;
const uint32_t skyTextureRes = 1024;
const uint32_t specularSkyProbeRes = 128;
const uint32_t diffuseSkyProbeRes = 4;
const uint32_t skyTextureMipCount = 8;
const uint32_t brdfLutRes = 512;
const uint32_t nHistogramBins = 128;
const int maxSunShadowCascadeCount = 4;

const uint32_t histogramTileSizeX = 32;
const uint32_t histogramTileSizeY = 32;

const uint32_t noiseTextureCount = 4;
const uint32_t noiseTextureWidth = 32;
const uint32_t noiseTextureHeight = 32;

const uint32_t shadowSampleCount = 8;

// bindings of global shader uniforms
const uint32_t globalUniformBufferBinding               = 0;
const uint32_t globalSamplerAnisotropicRepeatBinding    = 1;
const uint32_t globalSamplerNearestBlackBorderBinding   = 2;
const uint32_t globalSamplerLinearRepeatBinding         = 3;
const uint32_t globalSamplerLinearClampBinding          = 4;
const uint32_t globalSamplerNearestClampBinding         = 5;
const uint32_t globalSamplerLinearWhiteBorderBinding    = 6;
const uint32_t globalSamplerNearestRepeatBinding		= 7;
const uint32_t globalSamplerNearestWhiteBorderBinding	= 8;
const uint32_t globalNoiseTextureBindingBinding         = 9;

// currently rendering without attachment is not supported, so a small dummy is used
const ImageFormat dummyImageFormat = ImageFormat::R8;

// matrices used in the main rendering pass for depth-prepass and forward
struct MainPassMatrices {
    glm::mat4 model;
    glm::mat4 mvp;
    glm::mat4 mvpPrevious;
};

//---- function implementations ----

void resizeCallback(GLFWwindow* window, int width, int height) {
    RenderFrontend* frontEnd = reinterpret_cast<RenderFrontend*>(glfwGetWindowUserPointer(window));
    frontEnd->setResolution(width, height);
}

DefaultTextures createDefaultTextures() {
    DefaultTextures defaultTextures;
    // albedo
    {
        ImageDescription defaultDiffuseDesc;
        defaultDiffuseDesc.autoCreateMips = true;
        defaultDiffuseDesc.depth = 1;
        defaultDiffuseDesc.format = ImageFormat::RGBA8;
        defaultDiffuseDesc.manualMipCount = 1;
        defaultDiffuseDesc.mipCount = MipCount::FullChain;
        defaultDiffuseDesc.type = ImageType::Type2D;
        defaultDiffuseDesc.usageFlags = ImageUsageFlags::Sampled;
        defaultDiffuseDesc.width = 1;
        defaultDiffuseDesc.height = 1;

        const uint8_t initialData[4] = { 255, 255, 255, 255 };
        defaultTextures.diffuse = gRenderBackend.createImage(defaultDiffuseDesc, initialData, sizeof(initialData));
    }
    // specular
    {
        ImageDescription defaultSpecularDesc;
        defaultSpecularDesc.autoCreateMips = true;
        defaultSpecularDesc.depth = 1;
        defaultSpecularDesc.format = ImageFormat::RGBA8;
        defaultSpecularDesc.manualMipCount = 1;
        defaultSpecularDesc.mipCount = MipCount::FullChain;
        defaultSpecularDesc.type = ImageType::Type2D;
        defaultSpecularDesc.usageFlags = ImageUsageFlags::Sampled;
        defaultSpecularDesc.width = 1;
        defaultSpecularDesc.height = 1;

        const uint8_t initialData[4] = { 0, 128, 255, 0 };
        defaultTextures.specular = gRenderBackend.createImage(defaultSpecularDesc, initialData, sizeof(initialData));
    }
    // normal
    {
        ImageDescription defaultNormalDesc;
        defaultNormalDesc.autoCreateMips = true;
        defaultNormalDesc.depth = 1;
        defaultNormalDesc.format = ImageFormat::RG8;
        defaultNormalDesc.manualMipCount = 1;
        defaultNormalDesc.mipCount = MipCount::FullChain;
        defaultNormalDesc.type = ImageType::Type2D;
        defaultNormalDesc.usageFlags = ImageUsageFlags::Sampled;
        defaultNormalDesc.width = 1;
        defaultNormalDesc.height = 1;

        const uint8_t initialData[2] = { 128, 128 };
        defaultTextures.normal = gRenderBackend.createImage(defaultNormalDesc, initialData, sizeof(initialData));
    }
    // sky
    {
        ImageDescription defaultCubemapDesc;
        defaultCubemapDesc.autoCreateMips = true;
        defaultCubemapDesc.depth = 1;
        defaultCubemapDesc.format = ImageFormat::RGBA8;
        defaultCubemapDesc.manualMipCount = 1;
        defaultCubemapDesc.mipCount = MipCount::FullChain;
        defaultCubemapDesc.type = ImageType::Type2D;
        defaultCubemapDesc.usageFlags = ImageUsageFlags::Sampled;
        defaultCubemapDesc.width = 1;
        defaultCubemapDesc.height = 1;

        const uint8_t initialData[4] = { 255, 255, 255, 255 };
        defaultTextures.sky = gRenderBackend.createImage(defaultCubemapDesc, initialData, sizeof(initialData));
    }
    return defaultTextures;
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

    m_sky.init();
    m_bloom.init(width, height);
    m_volumetrics.init(width, height);
    m_taa.init(width, height, m_taaSettings);
    m_sdfGi.init(glm::ivec2(width, height), m_sdfTraceSettings, m_sdfDebugSettings, m_shadingConfig.sunShadowCascadeCount - 1);

    setupGlobalShaderInfoResources();

    gRenderBackend.newFrame();
    computeBRDFLut();
    gRenderBackend.prepareForDrawcallRecording();
    gRenderBackend.renderFrame(false);
}

void RenderFrontend::shutdown() {

}

void RenderFrontend::prepareNewFrame() {
    if (m_didResolutionChange) {

        gRenderBackend.recreateSwapchain(m_screenWidth, m_screenHeight, m_window);
        // full res render targets
        gRenderBackend.resizeImages({
            m_frameRenderTargets[0].motionBuffer,
            m_frameRenderTargets[1].motionBuffer,
            m_frameRenderTargets[0].colorBuffer,
            m_frameRenderTargets[0].depthBuffer,
            m_frameRenderTargets[1].colorBuffer,
            m_frameRenderTargets[1].depthBuffer,
            m_postProcessBuffers[0],
            m_postProcessBuffers[1],
            m_worldSpaceNormalImage 
            },
            m_screenWidth, m_screenHeight);
        // half res render targets
        gRenderBackend.resizeImages({ m_minMaxDepthPyramid, m_depthHalfRes }, m_screenWidth / 2, m_screenHeight / 2);

        m_taa.resizeImages(m_screenWidth, m_screenHeight);
        m_volumetrics.resizeTextures(m_screenWidth, m_screenHeight);
        m_sdfGi.resize(m_screenWidth, m_screenHeight, m_sdfTraceSettings);

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
        // don't reset m_isMainPassShaderDescriptionStale, this is done when rendering as it's used to trigger lut recreation
    }

    if (m_taaSettingsChanged) {
        m_taa.updateSettings(m_taaSettings);
        m_taaSettingsChanged = false;
    }
    if (m_isSDFDebugShaderDescriptionStale) {
        m_sdfGi.updateSDFDebugSettings(m_sdfDebugSettings, m_shadingConfig.sunShadowCascadeCount - 1);
        m_isSDFDebugShaderDescriptionStale = false;
    }
    if (m_isSDFDiffuseTraceShaderDescriptionStale) {
        m_sdfGi.updateSDFTraceSettings(m_sdfTraceSettings, m_shadingConfig.sunShadowCascadeCount - 1);
        m_isSDFDiffuseTraceShaderDescriptionStale = false;
    }
    if (m_sdfTraceResolutionChanged) {
        gRenderBackend.waitForGpuIdle();  // TODO: better solution than brute force wait
        m_sdfGi.resize(m_screenWidth, m_screenHeight, m_sdfTraceSettings);
        m_globalShaderInfo.cameraCut = true;
        m_sdfTraceResolutionChanged = false;
    }
    if (m_isLightMatrixPassShaderDescriptionStale) {
        gRenderBackend.updateComputePassShaderDescription(m_lightMatrixPass, createLightMatrixShaderDescription());
        m_isLightMatrixPassShaderDescriptionStale = false;
    }

    gRenderBackend.updateShaderCode();
    gRenderBackend.newFrame();

    if (m_drawUI) {
        drawUi();
    }
    prepareRenderpasses();
    gRenderBackend.prepareForDrawcallRecording();

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
    globalLayout.samplerBindings.push_back(globalSamplerNearestWhiteBorderBinding);

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
        SamplerResource(m_sampler_nearestRepeat,        globalSamplerNearestRepeatBinding),
        SamplerResource(m_sampler_nearestWhiteBorder,   globalSamplerNearestWhiteBorderBinding)
    };
    gRenderBackend.setGlobalDescriptorSetResources(globalResources);
}

void RenderFrontend::prepareRenderpasses(){

    static int sceneRenderTargetIndex;
    const FrameRenderTargets previousRenderTarget = m_frameRenderTargets[sceneRenderTargetIndex];
    sceneRenderTargetIndex++;
    sceneRenderTargetIndex %= 2;
    const FrameRenderTargets currentRenderTarget = m_frameRenderTargets[sceneRenderTargetIndex];

    if (m_sdfDebugSettings.visualisationMode != SDFVisualisationMode::None) {
        renderDepthPrepass(currentRenderTarget.prepassFramebuffer);
        computeDepthPyramid(currentRenderTarget.depthBuffer);
        computeColorBufferHistogram(m_postProcessBuffers[0]);

        m_sky.updateTransmissionLut();
        computeExposure();
        m_sky.updateSkyLut(m_lightBuffer, m_atmosphereSettings);

        computeSunLightMatrices();
        renderSunShadowCascades();

        SDFTraceDependencies traceDependencies = fillOutSdfGiDependencies(currentRenderTarget, previousRenderTarget);

        m_sdfGi.renderSDFVisualization(m_postProcessBuffers[0], traceDependencies, 
            m_sdfDebugSettings, m_sdfTraceSettings);

        computeTonemapping(m_postProcessBuffers[0]);
        return;
    }

    if (m_isBRDFLutShaderDescriptionStale) {
        computeBRDFLut();
        m_isBRDFLutShaderDescriptionStale = false;
    }

    computeColorBufferHistogram(previousRenderTarget.colorBuffer);
    m_sky.updateTransmissionLut();
    computeExposure();
    m_sky.updateSkyLut(m_lightBuffer, m_atmosphereSettings);
    renderDepthPrepass(currentRenderTarget.prepassFramebuffer);
    computeDepthPyramid(currentRenderTarget.depthBuffer);
    computeSunLightMatrices();
    renderSunShadowCascades();
    if (m_shadingConfig.indirectLightingTech == IndirectLightingTech::SDFTrace) {
        if (m_sdfTraceSettings.halfResTrace) {
            downscaleDepth(currentRenderTarget);
        }

        const SDFTraceDependencies traceDependencies = fillOutSdfGiDependencies(currentRenderTarget, previousRenderTarget);
        m_sdfGi.computeIndirectLighting(traceDependencies, m_sdfTraceSettings);
    }

    const int maxShadowCascadeIndex = m_shadingConfig.sunShadowCascadeCount - 1;

    Volumetrics::Dependencies volumetricsDependencies;
    volumetricsDependencies.lightBuffer = m_lightBuffer;
    volumetricsDependencies.shadowMap = m_shadowMaps[maxShadowCascadeIndex];
    volumetricsDependencies.sunShadowInfoBuffer = m_sunShadowInfoBuffer;

    m_volumetrics.computeVolumetricLighting(m_volumetricsSettings, m_windSettings, volumetricsDependencies);

    renderForwardShading(currentRenderTarget.colorFramebuffer);
    RenderPassHandle skyParent = m_mainPass;
    if (m_renderBoundingBoxes) {
        renderDebugGeometry(currentRenderTarget.colorFramebuffer);
        skyParent = m_debugGeoPass;
    }

    Sky::SkyRenderingDependencies skyDependencies;
    skyDependencies.volumetricIntegrationVolume = m_volumetrics.getIntegrationVolume();
    skyDependencies.volumetricLightingSettingsUniforms = m_volumetrics.getVolumetricsInfoBuffer();
    skyDependencies.lightBuffer = m_lightBuffer;

    m_sky.renderSky(currentRenderTarget.colorFramebuffer, skyDependencies);

    ImageHandle currentSrc = currentRenderTarget.colorBuffer;

    if (m_taaSettings.enabled) {

        if (m_taaSettings.useSeparateSupersampling) {
            m_taa.computeTemporalSuperSampling(currentRenderTarget, previousRenderTarget, m_postProcessBuffers[0]);

            currentSrc = m_postProcessBuffers[0];
        }

        m_taa.computeTemporalFilter(currentSrc, currentRenderTarget, m_postProcessBuffers[1]);

        currentSrc = m_postProcessBuffers[1];
    }

    if (m_bloomSettings.enabled) {
        m_bloom.computeBloom(currentSrc, m_bloomSettings);
    }
    computeTonemapping(currentSrc);
}

void RenderFrontend::setResolution(const uint32_t width, const uint32_t height) {
    m_screenWidth = width;
    m_screenHeight = height;
    // avoid zeros when minimzed
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

    // jitter matrix for temporal supersampling
    if(m_taaSettings.enabled){
        const glm::vec2 pixelSize = glm::vec2(1.f / m_screenWidth, 1.f / m_screenHeight);

        const glm::vec2 jitterInPixels = m_taa.computeProjectionMatrixJitter();
        m_taa.updateTaaResolveWeights(jitterInPixels);
        
        m_globalShaderInfo.currentFrameCameraJitter = jitterInPixels * pixelSize;
        const glm::mat4 jitteredProjection = m_taa.applyProjectionMatrixJitter(projectionMatrix, m_globalShaderInfo.currentFrameCameraJitter);

        m_viewProjectionMatrix = jitteredProjection * viewMatrix;
    }
    else {
        m_globalShaderInfo.currentFrameCameraJitter = glm::vec2(0.f);
        m_viewProjectionMatrix = projectionMatrix * viewMatrix;
    }

    m_globalShaderInfo.viewProjectionPrevious = m_globalShaderInfo.viewProjection;
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

    std::vector<fs::path> imagePaths;
    const size_t texturesPerMesh = 4;
    imagePaths.reserve(meshes.size() * texturesPerMesh);
    for (const MeshBinary& mesh : meshes) {
        imagePaths.push_back(mesh.texturePaths.albedoTexturePath);
        imagePaths.push_back(mesh.texturePaths.normalTexturePath);
        imagePaths.push_back(mesh.texturePaths.specularTexturePath);
        imagePaths.push_back(mesh.texturePaths.sdfTexturePath);
    }
    const std::vector<ImageHandle> meshImageHandles = loadImagesFromPaths(imagePaths);

    assert(backendHandles.size() == meshes.size());
    for (size_t i = 0; i < backendHandles.size(); i++) {
        MeshHandleFrontend meshHandleFrontend;
        meshHandleFrontend.index = (uint32_t)m_frontendMeshes.size();
        meshHandlesFrontend.push_back(meshHandleFrontend);

        MeshFrontend meshFrontend;
        meshFrontend.backendHandle = backendHandles[i];

        const MeshBinary mesh = meshes[i];

        meshFrontend.localBB = mesh.boundingBox;
        meshFrontend.meanAlbedo = mesh.meanAlbedo;

        const size_t baseIndex = (size_t)4 * i;

        // material
        ImageHandle albedoHandle = meshImageHandles[baseIndex + 0];
        if (albedoHandle.index == invalidIndex) {
            albedoHandle = m_defaultTextures.diffuse;
        }
        ImageHandle normalHandle = meshImageHandles[baseIndex + 1];
        if (normalHandle.index == invalidIndex) {
            normalHandle = m_defaultTextures.normal;
        }
        ImageHandle specularHandle = meshImageHandles[baseIndex + 2];
        if (specularHandle.index == invalidIndex) {
            specularHandle = m_defaultTextures.specular;
        }
        ImageHandle sdfHandle = meshImageHandles[baseIndex + 3];

        meshFrontend.material.albedoTextureIndex = gRenderBackend.getImageGlobalTextureArrayIndex(albedoHandle);
        meshFrontend.material.normalTextureIndex = gRenderBackend.getImageGlobalTextureArrayIndex(normalHandle);
        meshFrontend.material.specularTextureIndex = gRenderBackend.getImageGlobalTextureArrayIndex(specularHandle);

        if (sdfHandle.index == invalidIndex) {
            meshFrontend.sdfTextureIndex = -1;
        }
        else {
            meshFrontend.sdfTextureIndex = (int)gRenderBackend.getImageGlobalTextureArrayIndex(sdfHandle);
        }

        m_frontendMeshes.push_back(meshFrontend);
    }
    return meshHandlesFrontend;
}

void RenderFrontend::prepareForDrawcalls() {
    if (m_minimized) {
        return;
    }
    // global shader info must be updated before drawcalls, else they would be invalidated by descriptor set update
    // cant update at frame start as camera data must be set before updaing global info
    updateGlobalShaderInfo();
}

void RenderFrontend::renderScene(const std::vector<RenderObject>& scene) {

    // if we prepare render commands without consuming them we will save up a huge amount of commands
    // to avoid this commands are not recorded if minmized
    if (m_minimized) {
        return;
    }

    assert(scene.size() < maxObjectCountMainScene);

    m_sdfGi.updateSDFScene(scene, m_frontendMeshes);

    m_currentMeshCount += (uint32_t)scene.size();

    const bool renderingSDFVisualisation = m_sdfDebugSettings.visualisationMode != SDFVisualisationMode::None;

    // main and prepass
    JobSystem::Counter recordingFinished;

    struct MainPassPushConstants {
        uint32_t albedoTextureIndex;
        uint32_t normalTextureIndex;
        uint32_t specularTextureIndex;
        uint32_t transformIndex;
    };
    // data needed in outer scope to keep data pointer in scope when executing job
    std::vector<MainPassPushConstants> mainPassPushConstants;	
    std::vector<MeshHandle> mainPassCulledMeshes;
    {
        std::vector<MainPassMatrices> mainPassMatrices;

        // frustum culling
        for (const RenderObject& obj : scene) {

            const bool isVisible = isAxisAlignedBoundingBoxIntersectingViewFrustum(m_cameraFrustum, obj.bbWorld);

            if (isVisible) {
                m_currentMainPassDrawcallCount++;
                MeshFrontend meshFrontend = m_frontendMeshes[obj.mesh.index];
                mainPassCulledMeshes.push_back(meshFrontend.backendHandle);

                MainPassPushConstants meshPushConstants;
                meshPushConstants.albedoTextureIndex = meshFrontend.material.albedoTextureIndex;
                meshPushConstants.normalTextureIndex = meshFrontend.material.normalTextureIndex;
                meshPushConstants.specularTextureIndex = meshFrontend.material.specularTextureIndex;
                meshPushConstants.transformIndex = (uint32_t)mainPassMatrices.size();
                mainPassPushConstants.push_back(meshPushConstants);

                MainPassMatrices matrices;
                matrices.model = obj.modelMatrix;
                matrices.mvp = m_viewProjectionMatrix * obj.modelMatrix;
                matrices.mvpPrevious = m_previousViewProjectionMatrix * obj.previousModelMatrix;
                mainPassMatrices.push_back(matrices);
            }
        }
        // only prepass drawcalls needed for sdf debug visualisation
        if (renderingSDFVisualisation) {
            JobSystem::addJob([this, &mainPassCulledMeshes, &mainPassPushConstants](int workerIndex) {
                gRenderBackend.drawMeshes(mainPassCulledMeshes, (char*)mainPassPushConstants.data(), m_depthPrePass, workerIndex);
            }, &recordingFinished);
        }
        else {
            JobSystem::addJob([this, &mainPassCulledMeshes, &mainPassPushConstants](int workerIndex) {
                gRenderBackend.drawMeshes(mainPassCulledMeshes, (char*)mainPassPushConstants.data(), m_mainPass, workerIndex);
            }, &recordingFinished);
            JobSystem::addJob([this, &mainPassCulledMeshes, &mainPassPushConstants](int workerIndex) {
                gRenderBackend.drawMeshes(mainPassCulledMeshes, (char*)mainPassPushConstants.data(), m_depthPrePass, workerIndex);
            }, &recordingFinished);
        }
        gRenderBackend.setStorageBufferData(m_mainPassTransformsBuffer, mainPassMatrices.data(), 
            sizeof(MainPassMatrices) * mainPassMatrices.size());
    }

    // shadow pass
    struct ShadowPushConstants {
        uint32_t albedoTextureIndex;
        uint32_t transformIndex;
    };
    std::vector<MeshHandle> shadowCulledMeshes;
    std::vector<ShadowPushConstants> shadowPushConstantData;
    {
        const glm::vec3 sunDirection = directionToVector(m_sunDirection);
        // we must not cull behind the shadow frustum near plane, as objects there cast shadows into the visible area
        // for now we simply offset the near plane points very far into the light direction
        // this means that all objects in that direction within the moved distance will intersect our frustum and aren't culled
        const float nearPlaneExtensionLength = 10000.f;
        const glm::vec3 nearPlaneOffset = sunDirection * nearPlaneExtensionLength;
        m_sunShadowFrustum.points.l_l_n += nearPlaneOffset;
        m_sunShadowFrustum.points.r_l_n += nearPlaneOffset;
        m_sunShadowFrustum.points.l_u_n += nearPlaneOffset;
        m_sunShadowFrustum.points.r_u_n += nearPlaneOffset;

        std::vector<glm::mat4> shadowModelMatrices;

        // coarse frustum culling for shadow rendering, assuming shadow frustum if fitted to camera frustum
        // actual frustum is fitted tightly to depth buffer values, but that is done on the GPU
        for (const RenderObject& obj : scene) {

            const bool isVisible = isAxisAlignedBoundingBoxIntersectingViewFrustum(m_sunShadowFrustum, obj.bbWorld);

            if (isVisible) {
                m_currentShadowPassDrawcallCount++;

                const MeshFrontend mesh = m_frontendMeshes[obj.mesh.index];
                shadowCulledMeshes.push_back(mesh.backendHandle);

                ShadowPushConstants pushConstants;
                pushConstants.albedoTextureIndex = mesh.material.albedoTextureIndex;
                pushConstants.transformIndex = (uint32_t)shadowModelMatrices.size();
                shadowPushConstantData.push_back(pushConstants);

                shadowModelMatrices.push_back(obj.modelMatrix);
            }
        }
        for (int shadowPass = 0; shadowPass < m_shadingConfig.sunShadowCascadeCount; shadowPass++) {
            JobSystem::addJob([this, shadowPass, &shadowCulledMeshes, &shadowPushConstantData](int workerIndex) {
                gRenderBackend.drawMeshes(shadowCulledMeshes, (char*)shadowPushConstantData.data(), m_shadowPasses[shadowPass], workerIndex);
            }, & recordingFinished);
        }
        gRenderBackend.setStorageBufferData(m_shadowPassTransformsBuffer, shadowModelMatrices.data(),
            sizeof(glm::mat4) * shadowModelMatrices.size());
    }

    // bounding boxes
    std::vector<MeshHandle> bbMeshHandles;
    std::vector<uint32_t> bbPushConstantData; // contains index to matrix

    if (m_renderBoundingBoxes && !renderingSDFVisualisation) {
        std::vector<glm::mat4> boundingBoxMatrices;
        for (const RenderObject& obj : scene) {
            // culling again is repeating work
            // but keeps code nicely separated and is for debugging only
            const bool isVisible = isAxisAlignedBoundingBoxIntersectingViewFrustum(m_cameraFrustum, obj.bbWorld);
            if (isVisible) {
                const glm::vec3 offset = (obj.bbWorld.min + obj.bbWorld.max) * 0.5f;
                const glm::mat4 translationMatrix = glm::translate(glm::mat4(1.f), offset);
                const glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.f), (obj.bbWorld.max - obj.bbWorld.min) * 0.5f);
                const glm::mat4 bbMatrix = m_globalShaderInfo.viewProjection * translationMatrix * scaleMatrix;
                bbPushConstantData.push_back((uint32_t)boundingBoxMatrices.size());
                boundingBoxMatrices.push_back(bbMatrix);
                bbMeshHandles.push_back(m_boundingBoxMesh);
            }
        }
        JobSystem::addJob([this, &bbMeshHandles, &bbPushConstantData](int workerIndex) {
            gRenderBackend.drawMeshes(bbMeshHandles, (char*)bbPushConstantData.data(), m_debugGeoPass, workerIndex);
        }, & recordingFinished);
        const size_t bbMatricesSize = boundingBoxMatrices.size() * sizeof(glm::mat4);
        gRenderBackend.setStorageBufferData(m_boundingBoxDebugRenderMatrices, (char*)boundingBoxMatrices.data(), bbMatricesSize);
    }
    JobSystem::waitOnCounter(recordingFinished);
}

void RenderFrontend::renderFrame() {

    if (m_minimized) {
        return;
    }

    // sdf visualisation is written with compute without depth, so sky is not rendered separately
    if (m_sdfDebugSettings.visualisationMode == SDFVisualisationMode::None) {
        m_sky.issueSkyDrawcalls(m_sunDirection, m_viewProjectionMatrix);
    }

    m_globalShaderInfo.frameIndex++;
    m_globalShaderInfo.frameIndexMod2 = m_globalShaderInfo.frameIndex % 2;
    m_globalShaderInfo.frameIndexMod3 = m_globalShaderInfo.frameIndex % 3;
    m_globalShaderInfo.frameIndexMod4 = m_globalShaderInfo.frameIndex % 4;

    gRenderBackend.renderFrame(true);

    // set after frame finished so logic before rendering can decide if cut should happen
    m_globalShaderInfo.cameraCut = false;
}

void RenderFrontend::computeColorBufferHistogram(const ImageHandle lastFrameColor) const {

    StorageBufferResource histogramPerTileResource(m_histogramPerTileBuffer, false, 0);
    StorageBufferResource histogramResource(m_histogramBuffer, false, 1);

    // histogram per tile
    {
        ImageResource colorTextureResource(lastFrameColor, 0, 2);
        StorageBufferResource lightBufferResource(m_lightBuffer, true, 3);

        ComputePassExecution histogramPerTileExecution;
        histogramPerTileExecution.genericInfo.handle = m_histogramPerTilePass;
        histogramPerTileExecution.genericInfo.resources.storageBuffers = { histogramPerTileResource, lightBufferResource };
        histogramPerTileExecution.genericInfo.resources.sampledImages = { colorTextureResource };
        histogramPerTileExecution.dispatchCount[0] = uint32_t(std::ceilf((float)m_screenWidth / float(histogramTileSizeX)));
        histogramPerTileExecution.dispatchCount[1] = uint32_t(std::ceilf((float)m_screenHeight / float(histogramTileSizeY)));
        histogramPerTileExecution.dispatchCount[2] = 1;

        gRenderBackend.setComputePassExecution(histogramPerTileExecution);
    }

    const float binsPerDispatch = 64.f;
    // reset global tile
    {
        ComputePassExecution histogramResetExecution;
        histogramResetExecution.genericInfo.handle = m_histogramResetPass;
        histogramResetExecution.genericInfo.resources.storageBuffers = { histogramResource };
        histogramResetExecution.dispatchCount[0] = uint32_t(std::ceilf(float(nHistogramBins) / binsPerDispatch));
        histogramResetExecution.dispatchCount[1] = 1;
        histogramResetExecution.dispatchCount[2] = 1;

        gRenderBackend.setComputePassExecution(histogramResetExecution);
    }
    // combine tiles
    {
        ComputePassExecution histogramCombineTilesExecution;
        histogramCombineTilesExecution.genericInfo.handle = m_histogramCombinePass;
        histogramCombineTilesExecution.genericInfo.resources.storageBuffers = { histogramPerTileResource, histogramResource };
        uint32_t tileCount =
            (uint32_t)std::ceilf(m_screenWidth / float(histogramTileSizeX)) *
            (uint32_t)std::ceilf(m_screenHeight / float(histogramTileSizeY));
        histogramCombineTilesExecution.dispatchCount[0] = tileCount;
        histogramCombineTilesExecution.dispatchCount[1] = uint32_t(std::ceilf(float(nHistogramBins) / binsPerDispatch));
        histogramCombineTilesExecution.dispatchCount[2] = 1;

        gRenderBackend.setComputePassExecution(histogramCombineTilesExecution);
    }
}

void RenderFrontend::toggleUI() {
    m_drawUI = !m_drawUI;
}

void RenderFrontend::renderSunShadowCascades() const {
    for (int i = 0; i < m_shadingConfig.sunShadowCascadeCount; i++) {
        GraphicPassExecution shadowPassExecution;
        shadowPassExecution.genericInfo.handle = m_shadowPasses[i];
        shadowPassExecution.framebuffer = m_shadowCascadeFramebuffers[i];
        shadowPassExecution.genericInfo.resources.storageBuffers = {
            StorageBufferResource(m_sunShadowInfoBuffer, true, 0),
            StorageBufferResource(m_shadowPassTransformsBuffer, true, 1)
        };

        gRenderBackend.setGraphicPassExecution(shadowPassExecution);
    }
}

void RenderFrontend::computeExposure() const {
    StorageBufferResource lightBufferResource(m_lightBuffer, false, 0);
    StorageBufferResource histogramResource(m_histogramBuffer, false, 1);
    ImageResource transmissionLutResource(m_sky.getTransmissionLut(), 0, 2);

    ComputePassExecution preExposeLightsExecution;
    preExposeLightsExecution.genericInfo.handle = m_preExposeLightsPass;
    preExposeLightsExecution.genericInfo.resources.storageBuffers = { histogramResource, lightBufferResource };
    preExposeLightsExecution.genericInfo.resources.sampledImages = { transmissionLutResource };
    preExposeLightsExecution.dispatchCount[0] = 1;
    preExposeLightsExecution.dispatchCount[1] = 1;
    preExposeLightsExecution.dispatchCount[2] = 1;

    gRenderBackend.setComputePassExecution(preExposeLightsExecution);
}

void RenderFrontend::renderDepthPrepass(const FramebufferHandle framebuffer) const {
    GraphicPassExecution prepassExe;
    prepassExe.genericInfo.handle = m_depthPrePass;
    prepassExe.framebuffer = framebuffer;
    prepassExe.genericInfo.resources.storageBuffers = { StorageBufferResource(m_mainPassTransformsBuffer, true, 0) };
    gRenderBackend.setGraphicPassExecution(prepassExe);
}

void RenderFrontend::computeDepthPyramid(const ImageHandle depthBuffer) const {
    ComputePassExecution exe;
    exe.genericInfo.handle = m_depthPyramidPass;

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

    exe.genericInfo.resources.sampledImages = { depthBufferResource, depthPyramidResource };

    StorageBufferResource syncBuffer(m_depthPyramidSyncBuffer, false, 16);
    exe.genericInfo.resources.storageBuffers = { syncBuffer };

    exe.genericInfo.resources.storageImages.reserve(maxMipCount);
    const uint32_t unusedMipCount = maxMipCount - mipCount;
    for (uint32_t i = 0; i < maxMipCount; i++) {
        const uint32_t mipLevel = i >= unusedMipCount ? i - unusedMipCount : 0;
        ImageResource pyramidMip(m_minMaxDepthPyramid, mipLevel, i);
        exe.genericInfo.resources.storageImages.push_back(pyramidMip);
    }
    gRenderBackend.setComputePassExecution(exe);
}

void RenderFrontend::computeSunLightMatrices() const{
    ComputePassExecution exe;
    exe.genericInfo.handle = m_lightMatrixPass;
    exe.dispatchCount[0] = 1;
    exe.dispatchCount[1] = 1;
    exe.dispatchCount[2] = 1;

    const uint32_t depthPyramidMipCount = mipCountFromResolution(m_screenWidth / 2, m_screenHeight / 2, 1);
    ImageResource depthPyramidLowestMipResource(m_minMaxDepthPyramid, depthPyramidMipCount - 1, 1);
    exe.genericInfo.resources.storageImages = { depthPyramidLowestMipResource };

    StorageBufferResource lightMatrixBuffer(m_sunShadowInfoBuffer, false, 0);
    exe.genericInfo.resources.storageBuffers = { lightMatrixBuffer };

    struct LightMatrixPushConstants {
        float highestCascadePaddingSize;
        float highestCascadeMinFarPlane;
    };

    LightMatrixPushConstants pushConstants;
    pushConstants.highestCascadePaddingSize = m_sdfTraceSettings.traceInfluenceRadius;
    pushConstants.highestCascadeMinFarPlane = m_volumetricsSettings.maxDistance;

    // if strict cutoff enabled all hits outside influence radius are discarded anyways
    if (!m_sdfTraceSettings.strictInfluenceRadiusCutoff) {
        pushConstants.highestCascadePaddingSize += m_sdfTraceSettings.additionalSunShadowMapPadding;
    }

    exe.pushConstants = dataToCharArray(&pushConstants, sizeof(pushConstants));

    gRenderBackend.setComputePassExecution(exe);
}

void RenderFrontend::downscaleDepth(const FrameRenderTargets& currentTarget) const {
    ComputePassExecution exe;
    exe.genericInfo.handle = m_depthDownscalePass;

    const float localThreadSize = 8.f;
    const glm::vec2 halfRes = glm::ivec2(m_screenWidth, m_screenHeight) / 2;
    const glm::ivec2 dispatchCount = glm::ivec2(glm::ceil(halfRes / localThreadSize));
    exe.dispatchCount[0] = dispatchCount.x;
    exe.dispatchCount[1] = dispatchCount.y;
    exe.dispatchCount[2] = 1;

    exe.genericInfo.resources.storageImages = {
        ImageResource(m_depthHalfRes, 0, 0)
    };
    exe.genericInfo.resources.sampledImages = {
        ImageResource(currentTarget.depthBuffer, 0, 1)
    };

    gRenderBackend.setComputePassExecution(exe);
}

void RenderFrontend::renderForwardShading(const FramebufferHandle framebuffer) const {

    const auto brdfLutResource = ImageResource(m_brdfLut, 0, 3);
    const auto lightBufferResource = StorageBufferResource(m_lightBuffer, true, 7);
    const auto lightMatrixBuffer = StorageBufferResource(m_sunShadowInfoBuffer, true, 8);

    GraphicPassExecution mainPassExecution;
    mainPassExecution.genericInfo.handle = m_mainPass;
    mainPassExecution.framebuffer = framebuffer;
    mainPassExecution.genericInfo.resources.storageBuffers = { lightBufferResource, lightMatrixBuffer,
        StorageBufferResource{m_mainPassTransformsBuffer, true, 17} };

    const SDFGI::IndirectLightingImages indirectLight = m_sdfGi.getIndirectLightingResults(m_sdfTraceSettings.halfResTrace);

    mainPassExecution.genericInfo.resources.sampledImages = { 
        brdfLutResource,
        ImageResource(indirectLight.Y_SH, 0, 15),
        ImageResource(indirectLight.CoCg, 0, 16),
        ImageResource(m_volumetrics.getIntegrationVolume(), 0, 18)
    };

    // add shadow map cascade resources
    // max count shadow maps are always allocated and bound, but may be unused
    for (uint32_t i = 0; i < maxSunShadowCascadeCount; i++) {
        const auto shadowMapResource = ImageResource(m_shadowMaps[i], 0, 9 + i);
        mainPassExecution.genericInfo.resources.sampledImages.push_back(shadowMapResource);
    }
    mainPassExecution.genericInfo.resources.uniformBuffers = {
        UniformBufferResource(m_volumetrics.getVolumetricsInfoBuffer(), 19)
    };

    gRenderBackend.setGraphicPassExecution(mainPassExecution);
}

void RenderFrontend::computeTonemapping(const ImageHandle& src) const {
    const auto swapchainInput = gRenderBackend.getSwapchainInputImage();
    ImageResource targetResource(swapchainInput, 0, 0);
    ImageResource colorBufferResource(src, 0, 1);

    ComputePassExecution tonemappingExecution;
    tonemappingExecution.genericInfo.handle = m_tonemappingPass;
    tonemappingExecution.genericInfo.resources.storageImages = { targetResource };
    tonemappingExecution.genericInfo.resources.sampledImages = { colorBufferResource };
    tonemappingExecution.dispatchCount[0] = (uint32_t)std::ceil(m_screenWidth / 8.f);
    tonemappingExecution.dispatchCount[1] = (uint32_t)std::ceil(m_screenHeight / 8.f);
    tonemappingExecution.dispatchCount[2] = 1;

    gRenderBackend.setComputePassExecution(tonemappingExecution);
}

void RenderFrontend::renderDebugGeometry(const FramebufferHandle framebuffer) const {
    GraphicPassExecution debugPassExecution;
    debugPassExecution.genericInfo.handle = m_debugGeoPass;
    debugPassExecution.framebuffer = framebuffer;
    debugPassExecution.genericInfo.resources.storageBuffers = { StorageBufferResource(m_boundingBoxDebugRenderMatrices, true, 0) };
    gRenderBackend.setGraphicPassExecution(debugPassExecution);
}

std::vector<ImageHandle> RenderFrontend::loadImagesFromPaths(const std::vector<std::filesystem::path>& imagePaths) {

    // create set of paths which need to be loaded
    std::unordered_set <std::string> requiredDataSet;
    for (const fs::path path : imagePaths) {
        if (path == "") {
            continue;
        }
        if (m_textureMap.find(path.string()) == m_textureMap.end()) {
            requiredDataSet.insert(path.string());
        }
    }

    // parallel load of required images
    JobSystem::Counter loadingFinised;
    std::mutex mapMutex;
    std::unordered_map<std::string, std::pair<ImageDescription, size_t>> pathToDescriptionMap;
    std::vector<std::vector<uint8_t>> imageDataList;
    imageDataList.resize(requiredDataSet.size());

    size_t dataIndex = 0;
    for (const std::string &path : requiredDataSet) {

        // disable workerIndex unused parameter warning
        #pragma warning( push )
        #pragma warning( disable : 4100)

        JobSystem::addJob([path, dataIndex, &pathToDescriptionMap, &mapMutex, &imageDataList](int workerIndex) {
            ImageDescription image;
            if (loadImage(path, true, &image, &imageDataList[dataIndex])) {
                mapMutex.lock();
                pathToDescriptionMap[path] = std::pair(image, dataIndex);
                mapMutex.unlock();
            }
        }, &loadingFinised);

        // reenable warning
        #pragma warning( pop )

        dataIndex++;
    }
    JobSystem::waitOnCounter(loadingFinised);

    // create images and store in map
    for (const fs::path path : requiredDataSet) {
        if (pathToDescriptionMap.find(path.string()) != pathToDescriptionMap.end()) {
            const std::pair<ImageDescription, size_t>& descriptionDataIndexPair = pathToDescriptionMap[path.string()];
            const std::vector<uint8_t>& data = imageDataList[descriptionDataIndexPair.second];
            m_textureMap[path.string()] = gRenderBackend.createImage(
                descriptionDataIndexPair.first,
                data.data(),
                data.size());
        }
    }

    // build result vector
    std::vector<ImageHandle> result;
    result.reserve(imagePaths.size());

    ImageHandle invalidHandle;
    invalidHandle.index = invalidIndex;

    for (const fs::path path : imagePaths) {
        if (path == "" || m_textureMap.find(path.string()) == m_textureMap.end()) {
            result.push_back(invalidHandle);
        }
        else {
            result.push_back(m_textureMap[path.string()]);
        }
    }
    return result;
}

void RenderFrontend::computeBRDFLut() {

    const auto brdfLutStorageResource = ImageResource(m_brdfLut, 0, 0);

    ComputePassExecution brdfLutExecution;
    brdfLutExecution.genericInfo.handle = m_brdfLutPass;
    brdfLutExecution.genericInfo.resources.storageImages = { brdfLutStorageResource };
    brdfLutExecution.dispatchCount[0] = brdfLutRes / 8;
    brdfLutExecution.dispatchCount[1] = brdfLutRes / 8;
    brdfLutExecution.dispatchCount[2] = 1;
    gRenderBackend.setComputePassExecution(brdfLutExecution);
}

void RenderFrontend::updateCameraFrustum() {
    m_cameraFrustum = computeViewFrustum(m_camera);

    // debug geo
    std::vector<glm::vec3> frustumPoints;
    std::vector<uint32_t> frustumIndices;

    frustumToLineMesh(m_cameraFrustum, &frustumPoints, &frustumIndices);
}

void RenderFrontend::updateShadowFrustum() {
    m_sunShadowFrustum = computeOrthogonalFrustumFittedToCamera(m_cameraFrustum, directionToVector(m_sunDirection));

    // debug geo
    std::vector<glm::vec3> frustumPoints;
    std::vector<uint32_t> frustumIndices;
    frustumToLineMesh(m_sunShadowFrustum, &frustumPoints, &frustumIndices);
}

HistogramSettings RenderFrontend::createHistogramSettings() {
    HistogramSettings settings;

    settings.minValue = 0.001f;
    settings.maxValue = 200000.f;

    uint32_t pixelsPerTile = histogramTileSizeX * histogramTileSizeX;
    settings.maxTileCount = 1920 * 1080 / pixelsPerTile; //FIXME: update buffer on rescale

    return settings;
}

SDFTraceDependencies RenderFrontend::fillOutSdfGiDependencies(const FrameRenderTargets& currentFrame, 
    const FrameRenderTargets& previousFrame) {

    SDFTraceDependencies traceDependencies;
    traceDependencies.currentFrame = currentFrame;
    traceDependencies.previousFrame = previousFrame;
    traceDependencies.cameraFrustum = m_cameraFrustum;
    traceDependencies.depthHalfRes = m_depthHalfRes;
    traceDependencies.worldSpaceNormals = m_worldSpaceNormalImage;
    traceDependencies.skyLut = m_sky.getSkyLut();
    traceDependencies.shadowMap = m_shadowMaps[m_shadingConfig.sunShadowCascadeCount - 1];
    traceDependencies.lightBuffer = m_lightBuffer;
    traceDependencies.sunShadowInfoBuffer = m_sunShadowInfoBuffer;
    traceDependencies.depthMinMaxPyramid = m_minMaxDepthPyramid;

    return traceDependencies;
}

GraphicPassShaderDescriptions RenderFrontend::createForwardPassShaderDescription(const ShadingConfig& config) {

    GraphicPassShaderDescriptions shaderDesc;
    shaderDesc.vertex.srcPathRelative = "triangle.vert";
    shaderDesc.fragment.srcPathRelative = "triangle.frag";

    // specialisation constants
    {
        auto& constants = shaderDesc.fragment.specialisationConstants;

        // diffuse BRDF
        constants.push_back({
            0,                                                                      //location
            dataToCharArray((void*)&config.diffuseBRDF, sizeof(config.diffuseBRDF)) //value
            });
        // direct specular multiscattering
        constants.push_back({
            1,                                                                                      //location
            dataToCharArray((void*)&config.directMultiscatter, sizeof(config.directMultiscatter))   //value
            });
        // use geometry AA
        constants.push_back({
            2,                                                                          //location
            dataToCharArray((void*)&config.useGeometryAA, sizeof(config.useGeometryAA)) //value
            });
        // indirect lighting tech
        constants.push_back({
            3,                                                                                                          //location
            dataToCharArray((void*)&m_shadingConfig.indirectLightingTech, sizeof(m_shadingConfig.indirectLightingTech)) //value
            });
        // sun shadow cascade count
        constants.push_back({
            4,                                                                                                            //location
            dataToCharArray((void*)&m_shadingConfig.sunShadowCascadeCount, sizeof(m_shadingConfig.sunShadowCascadeCount)) //value
            });
    }

    return shaderDesc;
}

ShaderDescription RenderFrontend::createBRDFLutShaderDescription(const ShadingConfig& config) {

    ShaderDescription desc;
    desc.srcPathRelative = "brdfLut.comp";

    // diffuse brdf specialisation constant
    desc.specialisationConstants.push_back({
        0,                                                                      // location
        dataToCharArray((void*)&config.diffuseBRDF, sizeof(config.diffuseBRDF)) // value
        });
    
    return desc;
}

ShaderDescription RenderFrontend::createLightMatrixShaderDescription() {
    ShaderDescription desc;
    desc.srcPathRelative = "lightMatrix.comp";
    // sun shadow cascade count
    desc.specialisationConstants.push_back({
        0,                                                                                                            //location
        dataToCharArray((void*)&m_shadingConfig.sunShadowCascadeCount, sizeof(m_shadingConfig.sunShadowCascadeCount)) //value
    });
    return desc;
}

void RenderFrontend::updateGlobalShaderInfo() {
    m_globalShaderInfo.sunDirection = glm::vec4(directionToVector(m_sunDirection), 0.f);
    m_globalShaderInfo.cameraPosPrevious = m_globalShaderInfo.cameraPos;
    m_globalShaderInfo.cameraPos = glm::vec4(m_camera.extrinsic.position, 1.f); 

    m_globalShaderInfo.deltaTime = Timer::getDeltaTimeFloat();
    m_globalShaderInfo.time = Timer::getTimeFloat();
    m_globalShaderInfo.nearPlane = m_camera.intrinsic.near;
    m_globalShaderInfo.farPlane = m_camera.intrinsic.far;

    m_globalShaderInfo.cameraRight      = glm::vec4(m_camera.extrinsic.right, 0);
    m_globalShaderInfo.cameraUp         = glm::vec4(m_camera.extrinsic.up, 0);
    m_globalShaderInfo.cameraForwardPrevious = m_globalShaderInfo.cameraForward;
    m_globalShaderInfo.cameraForward    = glm::vec4(m_camera.extrinsic.forward, 0);
    m_globalShaderInfo.cameraTanFovHalf = glm::tan(glm::radians(m_camera.intrinsic.fov) * 0.5f);
    m_globalShaderInfo.cameraAspectRatio = m_camera.intrinsic.aspectRatio;
    m_globalShaderInfo.screenResolution = glm::ivec2(m_screenWidth, m_screenHeight);

    // supersampling needs a lod bias as the derivatives are incorrect
    // see "Filmic SMAA", page 117
    // we spread 8 samples over a 2 pixel radius, resulting in an average distance of 2/8 = 0.25
    // the resulting diagonal distance between the samples is sqrt(0.25) = 0.5
    const float lodBiasSampleRadius = 0.5f;
    m_globalShaderInfo.mipBias = m_taaSettings.enabled && m_taaSettings.useMipBias ? glm::log2(lodBiasSampleRadius) : 0.f;

    gRenderBackend.setUniformBufferData(m_globalUniformBuffer, &m_globalShaderInfo, sizeof(m_globalShaderInfo));
}

void RenderFrontend::initImages() {
    // post process buffer
    {
        ImageDescription desc;
        desc.width = m_screenWidth;
        desc.height = m_screenHeight;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::R11G11B10_uFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 0;
        desc.autoCreateMips = false;

        m_postProcessBuffers[0] = gRenderBackend.createImage(desc, nullptr, 0);
        m_postProcessBuffers[1] = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // shadow map cascades
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

        m_shadowMaps.reserve(maxSunShadowCascadeCount);
        for (uint32_t i = 0; i < maxSunShadowCascadeCount; i++) {
            const auto shadowMap = gRenderBackend.createImage(desc, nullptr, 0);
            m_shadowMaps.push_back(shadowMap);
        }
    }
    // brdf LUT
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

        m_brdfLut = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // min/max depth pyramid
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

        m_minMaxDepthPyramid = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // noise textures
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
        const std::vector<uint8_t> blueNoiseData = 
            generateBlueNoiseTexture(glm::ivec2(noiseTextureWidth, noiseTextureHeight), channelCount);

        m_noiseTextures.push_back(gRenderBackend.createImage(desc, blueNoiseData.data(), sizeof(uint8_t) * blueNoiseData.size()));
        m_globalShaderInfo.noiseTextureIndices[i] = gRenderBackend.getImageGlobalTextureArrayIndex(m_noiseTextures.back());
    }
    // world space normal buffer
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

        m_worldSpaceNormalImage = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // half res depth
    {
        ImageDescription desc;
        desc.width = m_screenWidth / 2;
        desc.height = m_screenHeight / 2;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::R16_sFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
        desc.mipCount = MipCount::One;

        m_depthHalfRes = gRenderBackend.createImage(desc, nullptr, 0);
    }
}

void RenderFrontend::initSamplers(){

    // all samplers have maxMip set to 20, even when not using anisotropy
    // this ensures that shaders can use texelFetch and textureLod to access all mip levels

    // anisotropic wrap
    {
        SamplerDescription desc;
        desc.interpolation = SamplerInterpolation::Linear;
        desc.maxMip = 20;
        desc.useAnisotropy = true;
        desc.wrapping = SamplerWrapping::Repeat;
        desc.borderColor = SamplerBorderColor::White;

        m_sampler_anisotropicRepeat = gRenderBackend.createSampler(desc);
    }
    // nearest black border
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
    // linear repeat
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
    // linear clamp
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
    // nearest clamp
    {
        SamplerDescription desc;
        desc.interpolation = SamplerInterpolation::Nearest;
        desc.wrapping = SamplerWrapping::Clamp;
        desc.useAnisotropy = false;
        desc.maxMip = 20;
        desc.borderColor = SamplerBorderColor::Black;

        m_sampler_nearestClamp = gRenderBackend.createSampler(desc);
    }
    // linear White Border
    {
        SamplerDescription desc;
        desc.interpolation = SamplerInterpolation::Linear;
        desc.maxMip = 20;
        desc.useAnisotropy = false;
        desc.wrapping = SamplerWrapping::Color;
        desc.borderColor = SamplerBorderColor::White;

        m_sampler_linearWhiteBorder = gRenderBackend.createSampler(desc);
    }
    // nearest repeat
    {
        SamplerDescription desc;
        desc.interpolation = SamplerInterpolation::Nearest;
        desc.maxMip = 20;
        desc.useAnisotropy = false;
        desc.wrapping = SamplerWrapping::Repeat;
        desc.borderColor = SamplerBorderColor::Black;

        m_sampler_nearestRepeat = gRenderBackend.createSampler(desc);
    }
    // nearest white border
    {
        SamplerDescription desc;
        desc.interpolation = SamplerInterpolation::Nearest;
        desc.wrapping = SamplerWrapping::Color;
        desc.useAnisotropy = false;
        desc.maxAnisotropy = 0;
        desc.borderColor = SamplerBorderColor::White;
        desc.maxMip = 20;

        m_sampler_nearestWhiteBorder = gRenderBackend.createSampler(desc);
    }
}

void RenderFrontend::initFramebuffers() {
    // shadow map framebuffers
    for (int i = 0; i < maxSunShadowCascadeCount; i++) {
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
        // motion buffer
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

            m_frameRenderTargets[i].motionBuffer = gRenderBackend.createImage(desc, nullptr, 0);
        }
        // color buffer
        {
            ImageDescription desc;
            desc.width = m_screenWidth;
            desc.height = m_screenHeight;
            desc.depth = 1;
            desc.type = ImageType::Type2D;
            desc.format = ImageFormat::R11G11B10_uFloat;
            desc.usageFlags = ImageUsageFlags::Attachment | ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
            desc.mipCount = MipCount::One;
            desc.manualMipCount = 0;
            desc.autoCreateMips = false;

            m_frameRenderTargets[i].colorBuffer = gRenderBackend.createImage(desc, nullptr, 0);
        }
        // depth buffer
        {
            ImageDescription desc;
            desc.width = m_screenWidth;
            desc.height = m_screenHeight;
            desc.depth = 1;
            desc.type = ImageType::Type2D;
            desc.format = ImageFormat::Depth32;
            desc.usageFlags = ImageUsageFlags::Attachment | ImageUsageFlags::Sampled;
            desc.mipCount = MipCount::One;
            desc.manualMipCount = 0;
            desc.autoCreateMips = false;

            m_frameRenderTargets[i].depthBuffer = gRenderBackend.createImage(desc, nullptr, 0);
        }
        // color framebuffer
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
        // prepass
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
    // histogram buffer
    {
        StorageBufferDescription histogramBufferDesc;
        histogramBufferDesc.size = nHistogramBins * sizeof(uint32_t);
        m_histogramBuffer = gRenderBackend.createStorageBuffer(histogramBufferDesc);
    }
    // light buffer 
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
    // per tile histogram
    {
        StorageBufferDescription histogramPerTileBufferDesc;
        histogramPerTileBufferDesc.size = (size_t)histogramSettings.maxTileCount * nHistogramBins * sizeof(uint32_t);
        m_histogramPerTileBuffer = gRenderBackend.createStorageBuffer(histogramPerTileBufferDesc);
    }
    // depth pyramid syncing buffer
    {
        StorageBufferDescription desc;
        desc.size = sizeof(uint32_t);
        desc.initialData = { (uint32_t)0 };
        m_depthPyramidSyncBuffer = gRenderBackend.createStorageBuffer(desc);
    }
    // light matrix buffer
    {
        StorageBufferDescription desc;
        const size_t splitSize = sizeof(glm::vec4);
        const size_t lightMatrixSize = sizeof(glm::mat4) * maxSunShadowCascadeCount;
        const size_t scaleInfoSize = sizeof(glm::vec2) * maxSunShadowCascadeCount;
        desc.size = splitSize + lightMatrixSize + scaleInfoSize;
        m_sunShadowInfoBuffer = gRenderBackend.createStorageBuffer(desc);
    }
    // global uniform buffer
    {
        UniformBufferDescription desc;
        desc.size = sizeof(m_globalShaderInfo);
        m_globalUniformBuffer = gRenderBackend.createUniformBuffer(desc);
    }
    // main pass transforms buffer
    {
        StorageBufferDescription desc;
        desc.size = sizeof(MainPassMatrices) * maxObjectCountMainScene;
        m_mainPassTransformsBuffer = gRenderBackend.createStorageBuffer(desc);
    }
    // shadow pass transforms buffer
    {
        StorageBufferDescription desc;
        desc.size = sizeof(glm::mat4) * maxObjectCountMainScene;
        m_shadowPassTransformsBuffer = gRenderBackend.createStorageBuffer(desc);
    }
    // bounding box debug rendering matrices
    {
        StorageBufferDescription desc;
        desc.size = maxObjectCountMainScene * sizeof(glm::mat4);
        m_boundingBoxDebugRenderMatrices = gRenderBackend.createStorageBuffer(desc);
    }
}

void RenderFrontend::initMeshs() {
    // bounding box mesh
    // drawn using lines, so needs different positions than skybox
    {
        AxisAlignedBoundingBox normalizedBB;
        normalizedBB.min = glm::vec3(-1);
        normalizedBB.max = glm::vec3(1);

        std::vector<glm::vec3> positions;
        std::vector<uint32_t> indices;
        axisAlignedBoundingBoxToLineMesh(normalizedBB, &positions, &indices);

        MeshBinary binary;
        binary.boundingBox = normalizedBB;
        binary.indexCount  = (uint32_t)indices.size();
        binary.vertexCount = (uint32_t)positions.size();
        binary.vertexBuffer.resize(sizeof(glm::vec3) * positions.size());
        memcpy(binary.vertexBuffer.data(), positions.data(), binary.vertexBuffer.size());
        // conversion to 16 bit index
        for (const uint32_t& index : indices) {
            binary.indexBuffer.push_back((uint16_t)index);
        }

        m_boundingBoxMesh = gRenderBackend.createMeshes({ binary }).back();
    }
}

void RenderFrontend::initRenderpasses(const HistogramSettings& histogramSettings) {
    // main shading pass
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
    // shadow cascade passes
    for (uint32_t cascade = 0; cascade < maxSunShadowCascadeCount; cascade++) {

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

        // cascade index specialisation constant
        shadowPassConfig.shaderDescriptions.vertex.specialisationConstants = { {
            0,                                                  // location
            dataToCharArray((void*)&cascade, sizeof(cascade))   // value
            }};
        

        const auto shadowPass = gRenderBackend.createGraphicPass(shadowPassConfig);
        m_shadowPasses.push_back(shadowPass);
    }
    // BRDF Lut creation pass
    {
        ComputePassDescription brdfLutPassDesc;
        brdfLutPassDesc.name = "BRDF Lut creation";
        brdfLutPassDesc.shaderDescription = createBRDFLutShaderDescription(m_shadingConfig);
        m_brdfLutPass = gRenderBackend.createComputePass(brdfLutPassDesc);
    }
    // geometry debug pass
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
    // histogram per tile pass
    {
        ComputePassDescription histogramPerTileDesc;
        histogramPerTileDesc.name = "Histogram per tile";
        histogramPerTileDesc.shaderDescription.srcPathRelative = "histogramPerTile.comp";

        const uint32_t maxTilesSpecialisationConstantID = 4;

        // specialisation constants
        {
            auto& constants = histogramPerTileDesc.shaderDescription.specialisationConstants;

            // bin count
            constants.push_back({
                0,                                                              // location
                dataToCharArray((void*)&nHistogramBins, sizeof(nHistogramBins)) // value
                });
            // min luminance constant
            constants.push_back({
                1,                                                                                      // location
                dataToCharArray((void*)&histogramSettings.minValue, sizeof(histogramSettings.minValue)) // value
                });
            // max luminance constant
            constants.push_back({
                2,                                                                                      // location
                dataToCharArray((void*)&histogramSettings.maxValue, sizeof(histogramSettings.maxValue)) // value
                });
            constants.push_back({
                3,                                                                                              // location
                dataToCharArray((void*)&histogramSettings.maxTileCount, sizeof(histogramSettings.maxTileCount)) // value
                });
        }
        m_histogramPerTilePass = gRenderBackend.createComputePass(histogramPerTileDesc);
    }
    // histogram reset pass
    {
        ComputePassDescription resetDesc;
        resetDesc.name = "Histogram reset";
        resetDesc.shaderDescription.srcPathRelative = "histogramReset.comp";

        // bin count constant
        resetDesc.shaderDescription.specialisationConstants.push_back({
            0,                                                              // location
            dataToCharArray((void*)&nHistogramBins, sizeof(nHistogramBins)) // value
            });

        m_histogramResetPass = gRenderBackend.createComputePass(resetDesc);
    }
    // histogram combine tiles pass
    {
        const uint32_t maxTilesSpecialisationConstantID = 1;

        ComputePassDescription histogramCombineDesc;
        histogramCombineDesc.name = "Histogram combine tiles";
        histogramCombineDesc.shaderDescription.srcPathRelative = "histogramCombineTiles.comp";

        auto& constants = histogramCombineDesc.shaderDescription.specialisationConstants;

        // bin count
        constants.push_back({
            0,                                                              // location
            dataToCharArray((void*)&nHistogramBins, sizeof(nHistogramBins)) // value
                });
        // max luminance constant
        constants.push_back({
            1,                                                                                              // location
            dataToCharArray((void*)&histogramSettings.maxTileCount, sizeof(histogramSettings.maxTileCount)) // value
                });

        m_histogramCombinePass = gRenderBackend.createComputePass(histogramCombineDesc);
    }
    // pre-expose lights pass
    {
        ComputePassDescription preExposeLightsDesc;
        preExposeLightsDesc.name = "Pre-expose lights";
        preExposeLightsDesc.shaderDescription.srcPathRelative = "preExposeLights.comp";

        // specialisation constants
        {
            auto& constants = preExposeLightsDesc.shaderDescription.specialisationConstants;

            // bin count
            constants.push_back({
                0,                                                              // location
                dataToCharArray((void*)&nHistogramBins, sizeof(nHistogramBins)) // value
                });
            // min luminance constant
            constants.push_back({
                1,                                                                                      // location
                dataToCharArray((void*)&histogramSettings.minValue,sizeof(histogramSettings.minValue))  // value
                });
            // max luminance constant
            constants.push_back({
                2,                                                                                      // location
                dataToCharArray((void*)&histogramSettings.maxValue, sizeof(histogramSettings.maxValue)) // value
                });
        }
        m_preExposeLightsPass = gRenderBackend.createComputePass(preExposeLightsDesc);
    }
    // depth prepass
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
    // depth pyramid pass
    {
        ComputePassDescription desc;
        desc.name = "Depth min/max pyramid creation";

        uint32_t threadgroupCount = 0;
        desc.shaderDescription = createDepthPyramidShaderDescription(&threadgroupCount);

        m_depthPyramidPass = gRenderBackend.createComputePass(desc);
    }
    // light matrix pass
    {
        ComputePassDescription desc;
        desc.name = "Compute light matrix";
        desc.shaderDescription = createLightMatrixShaderDescription();
        m_lightMatrixPass = gRenderBackend.createComputePass(desc);
    }
    // tonemapping pass
    {
        ComputePassDescription desc;
        desc.name = "Tonemapping";
        desc.shaderDescription.srcPathRelative = "tonemapping.comp";

        m_tonemappingPass = gRenderBackend.createComputePass(desc);
    }
    // depth downscale
    {
        ComputePassDescription desc;
        desc.name = "Depth downscale";
        desc.shaderDescription.srcPathRelative = "depthDownscale.comp";
        m_depthDownscalePass = gRenderBackend.createComputePass(desc);
    }
}

ShaderDescription RenderFrontend::createDepthPyramidShaderDescription(uint32_t* outThreadgroupCount) {

    ShaderDescription desc;
    desc.srcPathRelative = "depthHiZPyramid.comp";

    const uint32_t width = m_screenWidth / 2;
    const uint32_t height = m_screenHeight / 2;
    const uint32_t depthMipCount = mipCountFromResolution(width, height, 1);

    const uint32_t maxMipCount = 11; // see shader for details
    const auto dispatchCount = computeSinglePassMipChainDispatchCount(width, height, depthMipCount, maxMipCount);

    // mip count
    desc.specialisationConstants.push_back({ 
        0,                                                              // location
        dataToCharArray((void*)&depthMipCount, sizeof(depthMipCount))   // value
            });
    // depth buffer width
    desc.specialisationConstants.push_back({
        1,                                                              // location
        dataToCharArray((void*)&m_screenWidth, sizeof(m_screenWidth))   // value
            });
    // depth buffer height
    desc.specialisationConstants.push_back({
        2,                                                              // location
        dataToCharArray((void*)&m_screenHeight, sizeof(m_screenHeight)) // value
            });
    // threadgroup count
    *outThreadgroupCount = dispatchCount.x * dispatchCount.y;
    desc.specialisationConstants.push_back({
        3,                                                                          // location
        dataToCharArray((void*)outThreadgroupCount, sizeof(*outThreadgroupCount))   // value
            });

    return desc;
}

glm::ivec2 RenderFrontend::computeSinglePassMipChainDispatchCount(const uint32_t width, const uint32_t height, const uint32_t mipCount, const uint32_t maxMipCount) const {

    // shader can process up to 12 mip levels
    // thread group extent ranges from 16 to 1 depending on how many mips are used
    const uint32_t unusedMips = maxMipCount - mipCount;

    // last 6 mips are processed by single thread group
    if (unusedMips >= 6) {
        return glm::ivec2(1, 1);
    }
    else {
        // group size of 16x16 can compute up to a 32x32 area in mip0
        const uint32_t localThreadGroupExtent = 32 / (uint32_t)pow((uint32_t)2, unusedMips);

        glm::ivec2 count;
        count.x = (uint32_t)std::ceil(float(width) / localThreadGroupExtent);
        count.y = (uint32_t)std::ceil(float(height) / localThreadGroupExtent);

        return count;
    }
}

void RenderFrontend::drawUi() {

    m_renderTimingTimeSinceLastUpdate += m_globalShaderInfo.deltaTime;
    const bool updateTimings = m_renderTimingTimeSinceLastUpdate > m_renderTimingUpdateFrequency;
    if (updateTimings) {
        m_latestCPUTimeStatMs = gRenderBackend.getLastFrameCPUTime() * 1000;
        m_latestDeltaTimeStatMs = m_globalShaderInfo.deltaTime * 1000;
    }
    // rendering stats
    {
        ImGui::Begin("Rendering stats");
        ImGui::Text(("DeltaTime: " + std::to_string(m_latestDeltaTimeStatMs) + "ms").c_str());
        ImGui::Text(("CPU Time: " + std::to_string(m_latestCPUTimeStatMs) + "ms").c_str());
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

    // pass timings shown in columns
    {
        if (updateTimings) {
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
            // limit number of decimal places to improve readability
            const size_t commaIndex = std::max(int(timing.timeMs) / 10, 1);
            const size_t decimalPlacesToKeep = 2;
            auto timeString = std::to_string(timing.timeMs);
            timeString = timeString.substr(0, commaIndex + 1 + decimalPlacesToKeep);
            ImGui::Text(timeString.c_str());
        }
    }
    ImGui::End();

    ImGui::Begin("Rendering");

    // SDF settings
    if (ImGui::CollapsingHeader("SDF settings")) {

        const char* sdfDebugOptions[] = { "None", "Visualize SDF", "Camera tile usage", "SDF Normals", "Raymarching count" };
        m_isSDFDebugShaderDescriptionStale
            = ImGui::Combo("SDF debug mode", (int*)&m_sdfDebugSettings.visualisationMode, sdfDebugOptions, 5);

        ImGui::InputFloat("Diffuse trace influence range", &m_sdfTraceSettings.traceInfluenceRadius);

        if (m_sdfDebugSettings.visualisationMode == SDFVisualisationMode::None) {
            m_isSDFDiffuseTraceShaderDescriptionStale |=
                ImGui::Checkbox("Strict influence radius cutoff", &m_sdfTraceSettings.strictInfluenceRadiusCutoff);
        }
        else {
            ImGui::Checkbox("Use influence radius for debug visualisation", &m_sdfDebugSettings.useInfluenceRadiusForDebug);
        }

        if (!m_sdfTraceSettings.strictInfluenceRadiusCutoff) {
            ImGui::InputFloat("Shadow map extra padding", &m_sdfTraceSettings.additionalSunShadowMapPadding);
        }
        if (m_sdfDebugSettings.visualisationMode == SDFVisualisationMode::CameraTileUsage) {
            ImGui::Checkbox("Camera tile usage with hi-Z culling", &m_sdfDebugSettings.showCameraTileUsageWithHiZ);
        }
    }
    // Temporal filter Settings
    if(ImGui::CollapsingHeader("Temporal filter settings")){

        if (ImGui::Checkbox("Enabled", &m_taaSettings.enabled)) {
            m_globalShaderInfo.cameraCut = true;
        }

        ImGui::Checkbox("Separate temporal supersampling", &m_taaSettings.useSeparateSupersampling);

        m_taaSettingsChanged |= ImGui::Checkbox("Clipping", &m_taaSettings.useClipping);
        m_taaSettingsChanged |= ImGui::Checkbox("Dilate motion vector", &m_taaSettings.useMotionVectorDilation);

        const char* historySamplingOptions[] = { "Bilinear", "Bicubic16Tap", "Bicubic9Tap", "Bicubic5Tap", "Bicubic1Tap" };
        m_taaSettingsChanged |= ImGui::Combo("Bicubic history sample",
            (int*)&m_taaSettings.historySamplingTech, historySamplingOptions, 5);

        m_taaSettingsChanged |= ImGui::Checkbox("Tonemap temporal filter input", &m_taaSettings.supersampleUseTonemapping);
        m_taaSettingsChanged |= ImGui::Checkbox("Tonemap temporal supersample input", &m_taaSettings.filterUseTonemapping);
        ImGui::Checkbox("Use mip bias", &m_taaSettings.useMipBias); //bias is set trough buffer, so no update is required on change
    }
    // volumetric lighting settings
    if (ImGui::CollapsingHeader("Volumetric lighting settings")) {
        static glm::vec2 windDirection = glm::vec2(100.f, 60.f);
        ImGui::DragFloat2("Wind direction", &windDirection.x);
        m_windSettings.vector = directionToVector(windDirection);

        ImGui::DragFloat("Wind speed", &m_windSettings.speed, 0.1f);

        ImGui::ColorPicker3("Scattering color", &m_volumetricsSettings.scatteringCoefficients.x);
        ImGui::DragFloat("Absorption", &m_volumetricsSettings.absorptionCoefficient, 0.1f, 0.f, 1.f);
        ImGui::InputFloat("Max distance", &m_volumetricsSettings.maxDistance);
        m_volumetricsSettings.maxDistance = glm::max(m_volumetricsSettings.maxDistance, 1.f);
        ImGui::DragFloat("Base density", &m_volumetricsSettings.baseDensity, 0.01f, 0.f, 1.f);
        ImGui::DragFloat("Density noise range", &m_volumetricsSettings.densityNoiseRange, 0.01f, 0.f, 1.f);
        ImGui::DragFloat("Density noise scale", &m_volumetricsSettings.densityNoiseScale, 0.1f, 0.f);
        ImGui::DragFloat("Phase function G", &m_volumetricsSettings.phaseFunctionG, 0.1f, -0.99f, 0.99f);
    }
    if (ImGui::CollapsingHeader("Sky settings")) {
        ImGui::InputFloat3("Rayleigh scattering coefficients km", &m_atmosphereSettings.scatteringRayleighGround.x);
        ImGui::InputFloat3("Rayleigh extinction coefficients km", &m_atmosphereSettings.extinctionRayleighGround.x);
        ImGui::InputFloat("Mie scattering coefficients km", &m_atmosphereSettings.scatteringMieGround);
        ImGui::InputFloat("Mie extinction coefficients km", &m_atmosphereSettings.extinctionMieGround);
        ImGui::InputFloat("Mie scattering exponent", &m_atmosphereSettings.mieScatteringExponent);
        ImGui::InputFloat3("Ozone extinction coefficients km", &m_atmosphereSettings.ozoneExtinction.x);
        ImGui::InputFloat("Earth radius km", &m_atmosphereSettings.earthRadius);
        ImGui::InputFloat("Atmosphere height km", &m_atmosphereSettings.atmosphereHeight);
    }
    if (ImGui::CollapsingHeader("Bloom settings")) {
        ImGui::Checkbox("Bloom enabled", &m_bloomSettings.enabled);
        ImGui::DragFloat("Bloom strength", &m_bloomSettings.strength, 0.05f, 0.f, 1.f);
        ImGui::DragFloat("Bloom blur radius", &m_bloomSettings.radius, 0.1f, 0.f, 5.f);
    }
    // lighting settings
    if(ImGui::CollapsingHeader("Lighting settings")){
        ImGui::DragFloat2("Sun direction", &m_sunDirection.x);
        ImGui::DragFloat("Exposure offset EV", &m_globalShaderInfo.exposureOffset, 0.1f);
        ImGui::DragFloat("Adaption speed EV/s", &m_globalShaderInfo.exposureAdaptionSpeedEvPerSec, 0.1f, 0.f);
        ImGui::InputFloat("Sun Illuminance Lux", &m_globalShaderInfo.sunIlluminanceLux);
    }
    // shading settings
    if (ImGui::CollapsingHeader("Shading settings")) {

        // naming and values rely on enum values being ordered same as names and indices being [0,3]
        const char* diffuseBRDFOptions[] = { "Lambert", "Disney", "CoD WWII", "Titanfall 2" };
        const bool diffuseBRDFChanged = ImGui::Combo("Diffuse BRDF",
            (int*)&m_shadingConfig.diffuseBRDF,
            diffuseBRDFOptions, 4);
        m_isMainPassShaderDescriptionStale |= diffuseBRDFChanged;
        m_isBRDFLutShaderDescriptionStale = diffuseBRDFChanged;

        // naming and values rely on enum values being ordered same as names and indices being [0,3]
        const char* directMultiscatterBRDFOptions[] = { "McAuley", "Simplified", "Scaled GGX lobe", "None" };
        m_isMainPassShaderDescriptionStale |= ImGui::Combo("Direct Multiscatter BRDF",
            (int*)&m_shadingConfig.directMultiscatter,
            directMultiscatterBRDFOptions, 4);

        const char* indirectLightingLabels[] = { "SDF trace", "Constant ambient" };
        m_isMainPassShaderDescriptionStale |= ImGui::Combo("Indirect lighting tech", (int*)&m_shadingConfig.indirectLightingTech,
            indirectLightingLabels, 2);

        m_isMainPassShaderDescriptionStale |= ImGui::Checkbox("Geometric AA", &m_shadingConfig.useGeometryAA);

        if (ImGui::Checkbox("Indirect lighting half resolution", &m_sdfTraceSettings.halfResTrace)) {
            m_sdfTraceResolutionChanged = true;
        }

        const bool shadowCascadeCountChanged =
            ImGui::InputInt("Sun shadow cascade count", &m_shadingConfig.sunShadowCascadeCount);
        m_shadingConfig.sunShadowCascadeCount = glm::clamp(m_shadingConfig.sunShadowCascadeCount, 1, maxSunShadowCascadeCount);

        m_isMainPassShaderDescriptionStale          |= shadowCascadeCountChanged;
        m_isLightMatrixPassShaderDescriptionStale   |= shadowCascadeCountChanged;
        m_isSDFDiffuseTraceShaderDescriptionStale   |= shadowCascadeCountChanged;
        m_isSDFDebugShaderDescriptionStale          |= shadowCascadeCountChanged;
    }
    // camera settings
    if (ImGui::CollapsingHeader("Camera settings")) {
        ImGui::InputFloat("Near plane", &m_camera.intrinsic.near);
        ImGui::InputFloat("Far plane", &m_camera.intrinsic.far);
    }
    if (ImGui::CollapsingHeader("Debug settings")) {
        ImGui::Checkbox("Render bounding boxes", &m_renderBoundingBoxes);
    }
    ImGui::End();
}