#pragma once
#include"pch.h"
#include "MeshData.h"
#include "Backend/RenderBackend.h"
#include "RenderHandles.h"
#include "Camera.h"

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

    /*
    returns image from file
    checks a map of all loaded images if it is avaible, returns existing image if possible
    */
    bool getImageFromPath(std::filesystem::path path, ImageHandle* outImageHandle);
    std::map<std::filesystem::path, ImageHandle> m_textureMap;

    void firstFramePreparation();
    void computeBRDFLut();

    /*
    stored for resizing
    */
    GLFWwindow* m_window;

    bool m_firstFrame = true;

    /*
    backend
    */
    RenderBackend m_backend;

    /*
    camera
    */
    Camera m_camera;

    /*
    light and camera values
    */
    float m_exposureOffset = 0.f;

    /*
    resources
    */
    GlobalShaderInfo m_globalShaderInfo;

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

    /*
    resources
    */
    const uint32_t m_shadowMapRes = 1024;
    const uint32_t m_skyTextureRes = 1024;
    const uint32_t m_specularProbeRes = 512;
    const uint32_t m_specularProbeMipCount = 6;
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

    SamplerHandle m_shadowSampler;
    SamplerHandle m_hdriSampler;
    SamplerHandle m_cubeSampler;
    SamplerHandle m_skySamplerWithMips;
    SamplerHandle m_lutSampler;
    SamplerHandle m_defaultTexelSampler;

    MeshHandle      m_skyCube;

    StorageBufferHandle m_histogramPerTileBuffer;
    StorageBufferHandle m_histogramBuffer;
    StorageBufferHandle m_lightBuffer; //contains previous exposure and exposured light values
    
    const int m_diffuseBRDFDefaultSelection = 3;

    //specilisation constant indices
    const int m_mainPassSpecilisationConstantDiffuseBRDFIndex = 0;
    const int m_mainPassSpecilisationConstantDirectMultiscatterBRDFIndex = 1;
    const int m_mainPassSpecilisationConstantUseIndirectMultiscatterBRDFIndex = 2;

    const int m_brdfLutSpecilisationConstantDiffuseBRDFIndex = 0;

    //cached to reuse to change for changig specialisation constants
    GraphicPassShaderDescriptions   m_mainPassShaderConfig;
    ShaderDescription               m_brdfLutPassShaderConfig;

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
    void createHistogramPasses();
    void createDefaultTextures();
    void createDefaultSamplers();

    //default textures
    ImageHandle m_defaultDiffuseTexture;
    ImageHandle m_defaultSpecularTexture;
    ImageHandle m_defaultNormalTexture;
    ImageHandle m_defaultSkyTexture;

    /*
    sun
    */
    glm::vec2 m_sunDirection = glm::vec2(-120.f, 150.f);

    void updateSun();

    /*
    ui
    */
    void drawUi();

    uint32_t    m_screenWidth;
    uint32_t    m_screenHeight;
    bool        m_didResolutionChange = false;
    bool        m_minimized = false;
};