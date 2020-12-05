#pragma once
#include"pch.h"
#include "MeshData.h"
#include "Backend/RenderBackend.h"
#include "RenderHandles.h"
#include "Camera.h"
#include "BoundingBox.h"
#include "ViewFrustum.h"

struct GLFWwindow;

struct StaticMesh {
    MeshHandle  backendHandle;
    glm::mat4   modelMatrix = glm::mat4(1.f);
    AxisAlignedBoundingBox bbWorldSpace;
};

//settings are passed as specialisation constans, so they need to be encoded as ints
struct HistogramSettings {
    float minValue;
    float maxValue;
    int maxTileCount;
};

//these enumb values must correspond to the shader values
enum class DiffuseBRDF : int { Lambert = 0, Disney = 1, CoDWWII = 2, Titanfall2 = 3};
enum class DirectSpecularMultiscattering : int { McAuley = 0, Simplified = 1, ScaledGGX = 2, None = 3};

struct ShadingConfig {
    DiffuseBRDF diffuseBRDF = DiffuseBRDF::CoDWWII;
    DirectSpecularMultiscattering directMultiscatter = DirectSpecularMultiscattering::McAuley;
    bool useIndirectMultiscatter = true;
    bool useGeometryAA = true;
    bool useSkyOcclusion = true;
    bool useSkyOcclusionDirection = true;
};

enum class HistorySamplingTech : int { Bilinear=0, Bicubic16Tap=1, Bicubic9Tap=2, Bicubic5Tap=3, Bicubic1Tap=4 };

struct TemporalFilterSettings {
    bool enabled = true;
    bool useSeparateSupersampling = true;
    bool useClipping = true;
    bool useMotionVectorDilation = true;
    HistorySamplingTech historySamplingTech = HistorySamplingTech::Bicubic1Tap;
    bool supersampleUseTonemapping = true;
    bool filterUseTonemapping = true;
    bool useMipBias = true;
};

struct SkyOcclusionRenderData {
    glm::mat4 shadowMatrix = glm::mat4(1.f);
    glm::vec4 extends = glm::vec4(0.f);   //w unused
    glm::vec4 sampleDirection = glm::vec4(0.f);
    glm::vec4 offset = glm::vec4(0.f);
    float weight = 0.f;
};

//everything in km
struct AtmosphereSettings {
    glm::vec3 scatteringRayleighGround = glm::vec3(0.0058f, 0.0135f, 0.0331f);
    float earthRadius = 6371;
    glm::vec3 extinctionRayleighGround = scatteringRayleighGround;
    float atmosphereHeight = 100;
    glm::vec3 ozoneExtinction = glm::vec3(0.000650f, 0.001881f, 0.000085f);
    float scatteringMieGround = 0.006f;
    float extinctionMieGround = 1.11f * scatteringMieGround;
    float mieScatteringExponent = 0.76f;
};

enum class ShaderResourceType { SampledImage, Sampler, StorageImage, StorageBuffer, UniformBuffer };

struct DefaultTextures {
    ImageHandle diffuse;
    ImageHandle specular;
    ImageHandle normal;
    ImageHandle sky;
};

//simple wrapper to keep all images and framebuffers used in a frame in one place
//simplifies keeping resources of multiples frames around for temporal techniques
//motion buffer is shared between all frames as it's never reused
struct FrameRenderTargets {
    ImageHandle colorBuffer;
    ImageHandle motionBuffer;
    ImageHandle depthBuffer;
    FramebufferHandle colorFramebuffer;
    FramebufferHandle motionFramebuffer;
};

class RenderFrontend {
public:
    RenderFrontend() {};
    void setup(GLFWwindow* window);
    void shutdown();
    void prepareNewFrame();
    void setResolution(const uint32_t width, const uint32_t height);
    void setCameraExtrinsic(const CameraExtrinsic& extrinsic);
    //static meshes are used for baking and thus cannot be moved
    void addStaticMeshes(const std::vector<MeshBinary>& meshData, const std::vector<glm::mat4>& transforms);

    //before call camera settings and such must be set
    //after call drawcalls can be made
    void prepareForDrawcalls();

    void renderStaticMeshes();
    void renderFrame();

    //compute sky occlusion for all static meshes
    //must be called between frames as it calls gRenderBackend.newFrame() and gRenderBackend.renderFrame();
    void bakeSkyOcclusion();

private:

    void setupGlobalShaderInfo();

    //declare render passes to backend
    //this has to be done before meshes drawcalls can be issued
    void prepareRenderpasses();

    //computes image histogram using compute shaders
    void computeColorBufferHistogram(const ImageHandle lastFrameColor) const;
    void renderSky(const bool drewDebugPasses, const FramebufferHandle framebuffer) const;
    void renderSunShadowCascades() const;
    void computeExposure() const;
    void renderDepthPrepass(const FramebufferHandle framebuffer) const;
    void computeDepthPyramid(const ImageHandle depthBuffer) const;
    void computeSunLightMatrices() const;
    void renderForwardShading(const std::vector<RenderPassHandle>& externalDependencies, const FramebufferHandle framebuffer) const;
    void copyHDRImage(const ImageHandle src, const ImageHandle dst, RenderPassHandle parent) const; //input must be R11G11B10
    void computeTemporalSuperSampling(const FrameRenderTargets& currentFrame, const FrameRenderTargets& lastFrame,
        const ImageHandle target, const RenderPassHandle parent) const;
    void computeTemporalFilter(const ImageHandle colorSrc, const FrameRenderTargets& currentFrame, const ImageHandle target, const RenderPassHandle parent,
        const ImageHandle historyBufferSrc, const ImageHandle historyBufferDst) const;
    void computeTonemapping(const RenderPassHandle parent, const ImageHandle& src) const;
    void renderDebugGeometry(const FramebufferHandle framebuffer) const;
    void issueSkyDrawcalls();

    //checks a map of all loaded images if it is avaible, returns existing image if possible    
    bool loadImageFromPath(std::filesystem::path path, ImageHandle* outImageHandle);
    std::unordered_map<std::string, ImageHandle> m_textureMap; //using string instead of path to use default string hash

    void computeBRDFLut();

    //diffuse and specular convolution of sky lut for image based lighting
    void skyIBLConvolution();

    uint32_t m_screenWidth = 800;
    uint32_t m_screenHeight = 600;

    //contains meshes created by createMeshes()
    std::vector<StaticMesh> m_staticMeshes;

    //drawcall stats
    uint32_t m_currentMeshCount = 0;                //mesh commands received
    uint32_t m_currentMainPassDrawcallCount = 0;    //executed after camera culling
    uint32_t m_currentShadowPassDrawcallCount = 0;  //executed after shadow frustum culling

    //timings are cached and not updated every frame to improve readability
    std::vector<RenderPassTime> m_currentRenderTimings;
    float m_renderTimingUpdateFrequency = 0.2f;
    float m_renderTimingTimeSinceLastUpdate = 0.f;

    bool m_didResolutionChange = false;
    bool m_minimized = false;
    bool m_drawStaticMeshesBBs = false; //debug rendering of bounding boxes
    bool m_freezeAndDrawCameraFrustum = false;
    bool m_drawShadowFrustum = false;

    //stored for resizing
    GLFWwindow* m_window = nullptr;
    GlobalShaderInfo m_globalShaderInfo;

    Camera m_camera;    
    glm::mat4 m_viewProjectionMatrix = glm::mat4(1.f);
    glm::mat4 m_viewProjectionMatrixJittered = glm::mat4(1.f);
    glm::mat4 m_previousViewProjectionMatrix = glm::mat4(1.f);

    ViewFrustum m_cameraFrustum;
    ViewFrustum m_sunShadowFrustum;

    float m_exposureOffset = 0.f;
    glm::ivec3 m_skyOcclusionVolumeRes = glm::ivec3(0);
    
    void updateCameraFrustum();
    void updateShadowFrustum();

    HistogramSettings createHistogramSettings();

    ShadingConfig m_shadingConfig;
    TemporalFilterSettings m_temporalFilterSettings;
    AtmosphereSettings m_atmosphereSettings;

    RenderPassHandle m_mainPass;
    std::vector<RenderPassHandle> m_shadowPasses;
    RenderPassHandle m_skyTransmissionLutPass;
    RenderPassHandle m_skyMultiscatterLutPass;
    RenderPassHandle m_skyLutPass;
    RenderPassHandle m_skyPass;
    RenderPassHandle m_sunSpritePass;
    RenderPassHandle m_toCubemapPass;
    RenderPassHandle m_skyDiffuseConvolutionPass;
    RenderPassHandle m_brdfLutPass;
    std::vector<RenderPassHandle> m_cubemapMipPasses;
    std::vector<RenderPassHandle> m_skySpecularConvolutionPerMipPasses;
    RenderPassHandle m_histogramPerTilePass;
    RenderPassHandle m_histogramCombinePass;
    RenderPassHandle m_histogramResetPass;
    RenderPassHandle m_preExposeLightsPass;
    RenderPassHandle m_debugGeoPass;
    RenderPassHandle m_depthPrePass;
    RenderPassHandle m_depthPyramidPass;
    RenderPassHandle m_lightMatrixPass;
    RenderPassHandle m_tonemappingPass;
    RenderPassHandle m_temporalSupersamplingPass;
    RenderPassHandle m_temporalFilterPass;
    RenderPassHandle m_skyShadowPass;
    RenderPassHandle m_skyOcclusionGatherPass;  //gathers visibility from sky shadow map
    RenderPassHandle m_hdrImageCopyPass;    //input must be R11G11B10
    RenderPassHandle m_colorToLuminancePass;

    uint32_t m_specularSkyProbeMipCount = 0;

    ImageHandle m_postProcessBuffers[2];
    ImageHandle m_historyBuffers[2];
    ImageHandle m_skyTexture;
    ImageHandle m_diffuseSkyProbe;
    ImageHandle m_specularSkyProbe;
    ImageHandle m_skyTransmissionLut;
    ImageHandle m_skyMultiscatterLut;
    ImageHandle m_skyLut;
    ImageHandle m_brdfLut;
    ImageHandle m_minMaxDepthPyramid;
    ImageHandle m_skyShadowMap;
    ImageHandle m_skyOcclusionVolume;
    ImageHandle m_sceneLuminance;
    ImageHandle m_lastFrameLuminance;

    std::vector<ImageHandle> m_noiseTextures;
    uint32_t m_noiseTextureIndex = 0;

    DefaultTextures m_defaultTextures;

    std::vector<ImageHandle> m_shadowMaps;

    SamplerHandle m_sampler_anisotropicRepeat;
    SamplerHandle m_sampler_nearestBlackBorder;
    SamplerHandle m_sampler_linearRepeat;
    SamplerHandle m_sampler_linearClamp;
    SamplerHandle m_sampler_nearestClamp;
    SamplerHandle m_sampler_linearWhiteBorder;

    FramebufferHandle m_shadowCascadeFramebuffers[4];
    FramebufferHandle m_skyShadowFramebuffer;

    FrameRenderTargets m_frameRenderTargets[2];

    MeshHandle m_skyCube;
    MeshHandle m_quad;

    DynamicMeshHandle m_cameraFrustumModel;
    DynamicMeshHandle m_shadowFrustumModel;
    std::vector<DynamicMeshHandle> m_staticMeshesBBDebugMeshes;   //bounding box debug meshes

    StorageBufferHandle m_histogramPerTileBuffer;
    StorageBufferHandle m_histogramBuffer;
    StorageBufferHandle m_lightBuffer;          //previous exposure and exposured light values
    StorageBufferHandle m_sunShadowInfoBuffer;  //light matrices and cascade splits
    StorageBufferHandle m_depthPyramidSyncBuffer;

    UniformBufferHandle m_globalUniformBuffer;
    UniformBufferHandle m_skyOcclusionDataBuffer;
    UniformBufferHandle m_atmosphereSettingsBuffer;
    UniformBufferHandle m_taaResolveWeightBuffer;

    GraphicPassShaderDescriptions createForwardPassShaderDescription(const ShadingConfig& config);
    ShaderDescription createBRDFLutShaderDescription(const ShadingConfig& config);
    ShaderDescription createTemporalFilterShaderDescription();
    ShaderDescription createTemporalSupersamplingShaderDescription();

    bool m_isMainPassShaderDescriptionStale = false;
    bool m_isBRDFLutShaderDescriptionStale = false;
    bool m_isTemporalFilterShaderDescriptionStale = false;
    bool m_isTemporalSupersamplingShaderDescriptionStale = false;

    void updateGlobalShaderInfo();

    void initImages();
    void initSamplers();

    //must be called after initImages and initRenderpasses
    void initFramebuffers();

    void initRenderTargets();

    void initBuffers(const HistogramSettings& histogramSettings);

    //must be called after initImages as images have to be created to be used as attachments
    void initRenderpasses(const HistogramSettings& histogramSettings);

    void initMeshs();

    //threadgroup count is needed as a pointer in a specialisation constant, so it must be from outer scope to stay valid
    ShaderDescription createDepthPyramidShaderDescription(uint32_t* outThreadgroupCount);
    glm::ivec2 computeSinglePassMipChainDispatchCount(const uint32_t width, const uint32_t height, const uint32_t mipCount, const uint32_t maxMipCount) const;

    glm::vec2 m_sunDirection = glm::vec2(0.f, 0.f);
   
    void drawUi();
};

extern RenderFrontend gRenderFrontend;