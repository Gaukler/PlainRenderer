#pragma once
#include"pch.h"
#include "MeshData.h"
#include "Backend/RenderBackend.h"
#include "RenderHandles.h"
#include "Camera.h"
#include "AABB.h"
#include "ViewFrustum.h"
#include "Runtime/RuntimeScene.h"
#include "MeshFrontend.h"

#include "Techniques/Bloom.h"
#include "Techniques/Sky.h"
#include "Techniques/TAA.h"
#include "Techniques/Volumetrics.h"
#include "Techniques/SDFGI.h"
#include "Runtime/Rendering/FrameRenderTargets.h"

struct GLFWwindow;

struct HistogramSettings {
    float minValue;
    float maxValue;
    int maxTileCount;
};

// these enumb values must correspond to the shader values
enum class DiffuseBRDF : int { Lambert = 0, Disney = 1, CoDWWII = 2, Titanfall2 = 3};
enum class DirectSpecularMultiscattering : int { McAuley = 0, Simplified = 1, ScaledGGX = 2, None = 3};
enum class IndirectLightingTech : int { SDFTrace, ConstantAmbient };

struct ShadingConfig {
    DiffuseBRDF diffuseBRDF = DiffuseBRDF::CoDWWII;
    DirectSpecularMultiscattering directMultiscatter = DirectSpecularMultiscattering::McAuley;
    IndirectLightingTech indirectLightingTech = IndirectLightingTech::SDFTrace;
    bool useGeometryAA = true;
    int sunShadowCascadeCount = 3;
};

enum class ShaderResourceType { SampledImage, Sampler, StorageImage, StorageBuffer, UniformBuffer };

struct DefaultTextures {
    ImageHandle diffuse;
    ImageHandle specular;
    ImageHandle normal;
    ImageHandle sky;
};

class RenderFrontend {
public:
    RenderFrontend() {};
    void setup(GLFWwindow* window);
    void shutdown();
    void prepareNewFrame();
    void setResolution(const uint32_t width, const uint32_t height);
    void setCameraExtrinsic(const CameraExtrinsic& extrinsic);

    std::vector<MeshHandleFrontend> registerMeshes(const std::vector<MeshBinary>& meshes);

    // before call camera settings and such must be set
    // after call drawcalls can be made
    void prepareForDrawcalls();

    void renderScene(const std::vector<RenderObject>& scene);
    void renderFrame();

    void toggleUI();

private:

    void setupGlobalShaderInfoLayout();
    void setupGlobalShaderInfoResources();

    // declare render passes to backend
    // this has to be done before meshes drawcalls can be issued
    void prepareRenderpasses();

    // computes image histogram using compute shaders
    void computeColorBufferHistogram(const ImageHandle lastFrameColor) const;
    void renderSunShadowCascades() const;
    void computeExposure() const;
    void renderDepthPrepass(const FramebufferHandle framebuffer) const;
    void computeDepthPyramid(const ImageHandle depthBuffer) const;
    void computeSunLightMatrices() const;
    void downscaleDepth(const FrameRenderTargets& currentTarget) const;
    void renderForwardShading(const FramebufferHandle framebuffer) const;
    void computeTonemapping(const ImageHandle& src) const;
    void renderDebugGeometry(const FramebufferHandle framebuffer) const;

    // load multiple images, loading from disk is parallel
    // checks a map of all loaded images if it is avaible, returns existing image if possible
    // if image could not be loaded ImageHandle.index is set to invalidIndex
    std::vector<ImageHandle> loadImagesFromPaths(const std::vector<std::filesystem::path>& imagePaths);
    std::unordered_map<std::string, ImageHandle> m_textureMap; //using string instead of path to use default string hash

    void computeBRDFLut();

    std::vector<MeshFrontend> m_frontendMeshes;

    uint32_t m_screenWidth = 800;
    uint32_t m_screenHeight = 600;

    // drawcall stats
    uint32_t m_currentMeshCount = 0;                // mesh commands received
    uint32_t m_currentMainPassDrawcallCount = 0;    // executed after camera culling
    uint32_t m_currentShadowPassDrawcallCount = 0;  // executed after shadow frustum culling

    // timings are cached and not updated every frame to improve readability
    std::vector<RenderPassTime> m_currentRenderTimings;
    float m_renderTimingUpdateFrequency = 0.2f;
    float m_renderTimingTimeSinceLastUpdate = 0.f;
    float m_latestCPUTimeStatMs = 0.f;      // not updated every frame, only use as performance metric
    float m_latestDeltaTimeStatMs = 0.f;    // not updated every frame, only use as performance metric

    glm::ivec3 m_sceneSDFResolution = glm::ivec3(1);

    bool m_didResolutionChange = false;
    bool m_minimized = false;
    bool m_renderBoundingBoxes = false;
    bool m_drawUI = true;

    // stored for resizing
    GLFWwindow* m_window = nullptr;
    GlobalShaderInfo m_globalShaderInfo;

    Camera m_camera;    
    glm::mat4 m_viewProjectionMatrix = glm::mat4(1.f);
    glm::mat4 m_viewProjectionMatrixJittered = glm::mat4(1.f);
    glm::mat4 m_previousViewProjectionMatrix = glm::mat4(1.f);

    ViewFrustum m_cameraFrustum;
    ViewFrustum m_sunShadowFrustum;

    float m_exposureOffset = 0.f;
    
    void updateCameraFrustum();
    void updateShadowFrustum();

    HistogramSettings createHistogramSettings();

    glm::vec2 m_sunDirection = glm::vec2(0.f, 0.f);

    ShadingConfig m_shadingConfig;

    WindSettings m_windSettings;

    BloomSettings m_bloomSettings;
    Bloom m_bloom;

    AtmosphereSettings m_atmosphereSettings;
    Sky m_sky;

    TAASettings m_taaSettings;
    TAA m_taa;

    RenderPassHandle m_mainPass;
    std::vector<RenderPassHandle> m_shadowPasses;

    VolumetricsSettings m_volumetricsSettings;
    Volumetrics m_volumetrics;

    SDFDebugSettings m_sdfDebugSettings;
    SDFTraceSettings m_sdfTraceSettings;
    SDFGI m_sdfGi;
    SDFTraceDependencies fillOutSdfGiDependencies(const FrameRenderTargets& currentFrame, 
        const FrameRenderTargets& previousFrame);

    RenderPassHandle m_brdfLutPass;
    RenderPassHandle m_histogramPerTilePass;
    RenderPassHandle m_histogramCombinePass;
    RenderPassHandle m_histogramResetPass;
    RenderPassHandle m_preExposeLightsPass;
    RenderPassHandle m_debugGeoPass;
    RenderPassHandle m_depthPrePass;
    RenderPassHandle m_depthPyramidPass;
    RenderPassHandle m_lightMatrixPass;
    RenderPassHandle m_tonemappingPass;
    RenderPassHandle m_depthDownscalePass;

    ImageHandle m_postProcessBuffers[2];
    ImageHandle m_brdfLut;
    ImageHandle m_minMaxDepthPyramid;
    ImageHandle m_worldSpaceNormalImage;
    ImageHandle m_depthHalfRes;

    std::vector<ImageHandle> m_noiseTextures;

    DefaultTextures m_defaultTextures;

    std::vector<ImageHandle> m_shadowMaps;

    SamplerHandle m_sampler_anisotropicRepeat;
    SamplerHandle m_sampler_nearestBlackBorder;
    SamplerHandle m_sampler_linearRepeat;
    SamplerHandle m_sampler_linearClamp;
    SamplerHandle m_sampler_nearestClamp;
    SamplerHandle m_sampler_linearWhiteBorder;
    SamplerHandle m_sampler_nearestRepeat;
    SamplerHandle m_sampler_nearestWhiteBorder;

    FramebufferHandle	m_shadowCascadeFramebuffers[4];
    FrameRenderTargets	m_frameRenderTargets[2];

    MeshHandle m_boundingBoxMesh;

    StorageBufferHandle m_histogramPerTileBuffer;
    StorageBufferHandle m_histogramBuffer;
    StorageBufferHandle m_lightBuffer;          // previous exposure and exposured light values
    StorageBufferHandle m_sunShadowInfoBuffer;  // light matrices and cascade splits
    StorageBufferHandle m_depthPyramidSyncBuffer;
    StorageBufferHandle m_mainPassTransformsBuffer;
    StorageBufferHandle m_shadowPassTransformsBuffer;
    StorageBufferHandle m_boundingBoxDebugRenderMatrices;

    UniformBufferHandle m_globalUniformBuffer;

    GraphicPassShaderDescriptions createForwardPassShaderDescription(const ShadingConfig& config);
    ShaderDescription createBRDFLutShaderDescription(const ShadingConfig& config);
    ShaderDescription createSDFDebugShaderDescription();
    ShaderDescription createSDFDiffuseTraceShaderDescription(const bool strictInfluenceRadiusCutoff);
    ShaderDescription createLightMatrixShaderDescription();

    bool m_isMainPassShaderDescriptionStale = false;
    bool m_isBRDFLutShaderDescriptionStale = false;
    bool m_taaSettingsChanged = false;
    bool m_isSDFDebugShaderDescriptionStale = false;
    bool m_isSDFDiffuseTraceShaderDescriptionStale = false;
    bool m_sdfTraceResolutionChanged = false;
    bool m_isLightMatrixPassShaderDescriptionStale = false;

    void updateGlobalShaderInfo();

    void initImages();
    void initSamplers();

    // must be called after initImages and initRenderpasses
    void initFramebuffers();

    void initRenderTargets();

    void initBuffers(const HistogramSettings& histogramSettings);

    // must be called after initImages as images have to be created to be used as attachments
    void initRenderpasses(const HistogramSettings& histogramSettings);

    void initMeshs();

    // threadgroup count is needed as a pointer in a specialisation constant, so it must be from outer scope to stay valid
    ShaderDescription createDepthPyramidShaderDescription(uint32_t* outThreadgroupCount);
    glm::ivec2 computeSinglePassMipChainDispatchCount(const uint32_t width, const uint32_t height, const uint32_t mipCount, const uint32_t maxMipCount) const;
   
    void drawUi();
};

extern RenderFrontend gRenderFrontend;