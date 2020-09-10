#pragma once
#include"pch.h"
#include "MeshData.h"
#include "Backend/RenderBackend.h"
#include "RenderHandles.h"
#include "Camera.h"
#include "BoundingBox.h"
#include "ViewFrustum.h"

struct GLFWwindow;

//FrontendMeshHandle are given out by createMeshes
//they are indices into m_meshStates
//as the frontend creates meshes for internal use (like the skybox) the do not correspong to the backend handles
//like all handles contained in a struct to enforce type safety
struct FrontendMeshHandle {
    uint32_t index = invalidIndex;
};

struct MeshState {
    MeshHandle  backendHandle;
    glm::mat4   modelMatrix = glm::mat4(1.f);
    glm::mat4   previousFrameModelMatrix = glm::mat4(1.f); //required for reprojection
    AxisAlignedBoundingBox bb;
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
    DiffuseBRDF diffuseBRDF = DiffuseBRDF::Titanfall2;
    DirectSpecularMultiscattering directMultiscatter = DirectSpecularMultiscattering::McAuley;
    bool useIndirectMultiscatter = true;
    bool useGeometryAA = true;
    bool useSkyOcclusion = true;
    bool useSkyOcclusionDirection = true;
};

struct TAASettings {
    bool useClipping = true;
    bool useVarianceClipping = true;
    bool useYCoCg = true;
    bool useMotionVectorDilation = true;
    float textureLoDBias = -0.75f;
};

struct SkyOcclusionRenderData {
    glm::mat4 shadowMatrix = glm::mat4(1.f);
    glm::vec4 extends = glm::vec4(0.f);   //w unused
    glm::vec4 sampleDirection = glm::vec4(0.f);
    glm::vec4 offset = glm::vec4(0.f);
    float weight = 0.f;
};

/*
shader resource types
define inputs and outputs of renderpass
*/
enum class ShaderResourceType { SampledImage, Sampler, StorageImage, StorageBuffer, UniformBuffer };

class RenderFrontend {
public:
    RenderFrontend() {};
    void setup(GLFWwindow* window);
    void shutdown();
    void newFrame();
    void setResolution(const uint32_t width, const uint32_t height);
    void setCameraExtrinsic(const CameraExtrinsic& extrinsic);
    std::vector<FrontendMeshHandle> createMeshes(const std::vector<MeshData>& meshData);
    void issueMeshDraws(const std::vector<FrontendMeshHandle>& meshs);
    void setModelMatrix(const FrontendMeshHandle handle, const glm::mat4& m);
    void renderFrame();

private:

    //checks a map of all loaded images if it is avaible, returns existing image if possible    
    bool loadImageFromPath(std::filesystem::path path, ImageHandle* outImageHandle);
    std::map<std::filesystem::path, ImageHandle> m_textureMap;

    void firstFramePreparation();
    void computeBRDFLut();

    //compute sky occlusion for all existing meshes
    //renders multiple frames, so all current render commands are consumed
    void computeSkyOcclusion();

    uint32_t m_screenWidth = 800;
    uint32_t m_screenHeight = 600;

    //contains meshes created by createMeshes()
    std::vector<MeshState> m_meshStates;

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
    bool m_firstFrame = true;
    bool m_drawBBs = false; //debug rendering of bounding boxes
    bool m_freezeAndDrawCameraFrustum = false;
    bool m_drawShadowFrustum = false;

    //stored for resizing
    GLFWwindow* m_window = nullptr;
    RenderBackend m_backend;
    GlobalShaderInfo m_globalShaderInfo;

    Camera m_camera;    
    glm::mat4 m_viewProjectionMatrix = glm::mat4(1.f);
    glm::mat4 m_viewProjectionMatrixJittered = glm::mat4(1.f);
    glm::mat4 m_previousViewProjectionMatrix = glm::mat4(1.f);

    ViewFrustum m_cameraFrustum;
    float m_exposureOffset = 0.f;
    glm::ivec3 m_skyOcclusionVolumeRes = glm::ivec3(0);
    
    void updateCameraFrustum();

    HistogramSettings createHistogramSettings();

    ShadingConfig m_shadingConfig;
    TAASettings m_taaSettings;

    /*
    passes
    */
    RenderPassHandle m_mainPass;
    std::vector<RenderPassHandle> m_shadowPasses;
    RenderPassHandle m_skyPass;
    RenderPassHandle m_toCubemapPass;
    RenderPassHandle m_diffuseConvolutionPass;
    RenderPassHandle m_brdfLutPass;
    std::vector<RenderPassHandle> m_cubemapMipPasses;
    std::vector<RenderPassHandle> m_specularConvolutionPerMipPasses;
    RenderPassHandle m_histogramPerTilePass;
    RenderPassHandle m_histogramCombinePass;
    RenderPassHandle m_histogramResetPass;
    RenderPassHandle m_preExposeLightsPass;
    RenderPassHandle m_debugGeoPass;
    RenderPassHandle m_depthPrePass;
    RenderPassHandle m_depthPyramidPass;
    RenderPassHandle m_lightMatrixPass;
    RenderPassHandle m_imageCopyHDRPass;
    RenderPassHandle m_tonemappingPass;
    RenderPassHandle m_taaPass;
    RenderPassHandle m_skyShadowPass;
    RenderPassHandle m_skyOcclusionGatherPass;  //gathers visibility from sky shadow map

    /*
    resources
    */
    uint32_t m_specularProbeMipCount = 0;

    const uint32_t m_shadowMapRes = 2048;
    const uint32_t m_skyTextureRes = 1024;
    const uint32_t m_specularProbeRes = 512;
    const uint32_t m_diffuseProbeRes = 256;
    const uint32_t m_skyTextureMipCount = 8;
    const uint32_t m_brdfLutRes = 512;
    const uint32_t m_nHistogramBins = 128;
    const uint32_t m_shadowCascadeCount = 4;

    const uint32_t m_histogramTileSizeX = 32;
    const uint32_t m_histogramTileSizeY = 32;

    const uint32_t m_skyShadowMapRes = 1024;
    const uint32_t m_skyOcclusionVolumeMaxRes = 256;
    const float m_skyOcclusionTargetDensity = 0.5f; //meter/texel
    const uint32_t skyOcclusionSampleCount = 1024;

    ImageHandle m_colorBuffer;
    ImageHandle m_depthBuffer;
    ImageHandle m_motionVectorBuffer;
    ImageHandle m_environmentMapSrc;
    ImageHandle m_skyTexture;
    ImageHandle m_diffuseProbe;
    ImageHandle m_specularProbe;
    ImageHandle m_brdfLut;
    ImageHandle m_minMaxDepthPyramid;
    ImageHandle m_historyBuffer;
    ImageHandle m_skyShadowMap;
    ImageHandle m_skyOcclusionVolume;

    std::vector<ImageHandle> m_shadowMaps;

    SamplerHandle m_shadowSampler;
    SamplerHandle m_hdriSampler;
    SamplerHandle m_cubeSampler;
    SamplerHandle m_skySamplerWithMips;
    SamplerHandle m_lutSampler;
    SamplerHandle m_defaultTexelSampler;
    SamplerHandle m_clampedDepthSampler;
    SamplerHandle m_colorSampler;
    SamplerHandle m_skyOcclusionSampler;

    MeshHandle m_skyCube;

    DynamicMeshHandle m_cameraFrustumModel;
    DynamicMeshHandle m_shadowFrustumModel;
    std::vector<DynamicMeshHandle> m_bbDebugMeshes; //bounding box debug mesh
    std::vector<AxisAlignedBoundingBox> m_bbsToDebugDraw; //bounding boxes for debug rendering this frame

    StorageBufferHandle m_histogramPerTileBuffer;
    StorageBufferHandle m_histogramBuffer;
    StorageBufferHandle m_lightBuffer; //contains previous exposure and exposured light values
    StorageBufferHandle m_sunShadowInfoBuffer; //contains light matrices and cascade splits
    StorageBufferHandle m_depthPyramidSyncBuffer;

    UniformBufferHandle m_skyOcclusionDataBuffer;

    GraphicPassShaderDescriptions createForwardPassShaderDescription(const ShadingConfig& config);
    ShaderDescription createBRDFLutShaderDescription(const ShadingConfig& config);
    ShaderDescription createTAAShaderDescription();

    bool m_isMainPassShaderDescriptionStale = false;
    bool m_isBRDFLutShaderDescriptionStale = false;
    bool m_isTAAShaderDescriptionStale = false;

    void updateGlobalShaderInfo();

    void initImages();
    void initSamplers();

    void initBuffers(const HistogramSettings& histogramSettings);

    //must be called after initImages as images have to be created to be used as attachments
    void initRenderpasses(const HistogramSettings& histogramSettings);

    //must be called after initRenderpasses as meshes are created in regards to pass
    void initMeshs();

    //threadgroup count is needed as a pointer in a specialisation constant, so it must be from outer scope to stay valid
    ShaderDescription createDepthPyramidShaderDescription(uint32_t* outThreadgroupCount);
    glm::ivec2 computeDepthPyramidDispatchCount();

    //default textures
    ImageHandle m_defaultDiffuseTexture;
    ImageHandle m_defaultSpecularTexture;
    ImageHandle m_defaultNormalTexture;
    ImageHandle m_defaultSkyTexture;

    //sun
    glm::vec2 m_sunDirection = glm::vec2(-120.f, 150.f);
    void updateSun();

    //ui    
    void drawUi();
};