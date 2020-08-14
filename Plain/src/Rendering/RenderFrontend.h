#pragma once
#include"pch.h"
#include "MeshData.h"
#include "Backend/RenderBackend.h"
#include "RenderHandles.h"
#include "Camera.h"
#include "BoundingBox.h"
#include "ViewFrustum.h"

struct GLFWwindow;


/*
shader resource types
define inputs and outputs of renderpass
*/
enum class ShaderResourceType { SampledImage, Sampler, StorageImage, StorageBuffer, UniformBuffer };

class RenderFrontend {
public:
    RenderFrontend() {};
    void setup(GLFWwindow* window);
    void teardown();
    void newFrame();
    void setResolution(const uint32_t width, const uint32_t height);
    void setCameraExtrinsic(const CameraExtrinsic& extrinsic);
    std::vector<MeshHandle> createMeshes(const std::vector<MeshData>& meshData);
    void issueMeshDraws(const std::vector<MeshHandle>& meshs, const std::vector<glm::mat4>& modelMatrices);
    void renderFrame();
private:

    //returns image from file
    //checks a map of all loaded images if it is avaible, returns existing image if possible    
    bool getImageFromPath(std::filesystem::path path, ImageHandle* outImageHandle);
    std::map<std::filesystem::path, ImageHandle> m_textureMap;

    void firstFramePreparation();
    void computeBRDFLut();

    uint32_t m_screenWidth;
    uint32_t m_screenHeight;

    uint32_t m_currentFrameMainPassDrawcallCount = 0;

    bool m_didResolutionChange = false;
    bool m_minimized = false;
    bool m_firstFrame = true;
    bool m_drawBBs = false; //debug rendering of bounding boxes
    bool m_freezeAndDrawCameraFrustum = false;

    //probably not the most efficient way
    //relying on the MeshHandle being indices could break if the backend changes or mesh deletion is added so use map for now
    std::unordered_map<MeshHandle, AxisAlignedBoundingBox> m_meshHandleToBoundingBox;

    //stored for resizing
    GLFWwindow* m_window;
    RenderBackend m_backend;
    GlobalShaderInfo m_globalShaderInfo;

    Camera m_camera;    
    ViewFrustum m_cameraFrustum;
    float m_exposureOffset = 0.f;
    
    void updateCameraFrustum();

    /*
    passes
    */
    RenderPassHandle m_mainPass;
    RenderPassHandle m_shadowPass;
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

    /*
    resources
    */
    uint32_t m_specularProbeMipCount = 0;

    const uint32_t m_shadowMapRes = 1024;
    const uint32_t m_skyTextureRes = 1024;
    const uint32_t m_specularProbeRes = 512;
    const uint32_t m_diffuseProbeRes = 256;
    const uint32_t m_skyTextureMipCount = 8;
    const uint32_t m_brdfLutRes = 512;
    const uint32_t m_nHistogramBins = 128;
    const float m_histogramMin = 0.001f;
    const float m_histogramMax = 200000.f;

    const uint32_t m_histogramTileSizeX = 32;
    const uint32_t m_histogramTileSizeY = 32;

    ImageHandle m_colorBuffer;
    ImageHandle m_depthBuffer;
    ImageHandle m_shadowMap;
    ImageHandle m_environmentMapSrc;
    ImageHandle m_skyTexture;
    ImageHandle m_diffuseProbe;
    ImageHandle m_specularProbe;
    ImageHandle m_brdfLut;
    ImageHandle m_minMaxDepthPyramid;

    SamplerHandle m_shadowSampler;
    SamplerHandle m_hdriSampler;
    SamplerHandle m_cubeSampler;
    SamplerHandle m_skySamplerWithMips;
    SamplerHandle m_lutSampler;
    SamplerHandle m_defaultTexelSampler;
    SamplerHandle m_clampedDepthSampler;

    MeshHandle m_skyCube;

    DynamicMeshHandle m_cameraFrustumModel;
    std::vector<DynamicMeshHandle> m_bbDebugMeshes; //bounding box debug mesh
    std::vector<AxisAlignedBoundingBox> m_bbsToDebugDraw; //bounding boxes for debug rendering this frame

    StorageBufferHandle m_histogramPerTileBuffer;
    StorageBufferHandle m_histogramBuffer;
    StorageBufferHandle m_lightBuffer; //contains previous exposure and exposured light values
    UniformBufferHandle m_depthPyramidSyncBuffer;
    
    const int m_diffuseBRDFDefaultSelection = 3;

    //specilisation constant indices
    const int m_mainPassSpecilisationConstantDiffuseBRDFIndex = 0;
    const int m_mainPassSpecilisationConstantDirectMultiscatterBRDFIndex = 1;
    const int m_mainPassSpecilisationConstantUseIndirectMultiscatterBRDFIndex = 2;
    const int m_mainPassSpecilisationConstantGeometricAAIndex = 3;

    const int m_brdfLutSpecilisationConstantDiffuseBRDFIndex = 0;

    const int m_depthPyramidSpecialisationConstantMipCountIndex = 0;
    const int m_depthPyramidSpecialisationConstantDepthResX = 1;
    const int m_depthPyramidSpecialisationConstantDepthResY = 2;
    const int m_depthPyramidSpecialisationConstantGroupCount = 3;

    //cached to reuse to change for changig specialisation constants
    GraphicPassShaderDescriptions   m_mainPassShaderConfig;
    ShaderDescription               m_brdfLutPassShaderConfig;
    ShaderDescription               m_depthPyramidShaderConfig;

    bool m_isMainPassShaderDescriptionStale = false;
    bool m_isBRDFLutShaderDescriptionStale = false;

    void updateGlobalShaderInfo();

    void createMainPass(const uint32_t width, const uint32_t height);
    void createShadowPass();
    void createSkyPass();
    void createSkyCubeMesh();
    void createSkyTexturePreparationPasses();
    void createDiffuseConvolutionPass();
    void createSpecularConvolutionPass();
    void createBRDFLutPreparationPass();
    void createDebugGeoPass();
    void createHistogramPasses();
    void createDepthPrePass();
    void createDepthPyramidPass();
    void createDefaultTextures();
    void createDefaultSamplers();

    //sets resolution dependant specialisation constants
    void updateDepthPyramidShaderDescription();
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