#pragma once
#include"pch.h"
#include "MeshData.h"
#include "Backend/RenderBackend.h"
#include "RenderHandles.h"
#include "Camera.h"
#include "AABB.h"
#include "ViewFrustum.h"
#include "Runtime/RuntimeScene.h"

struct GLFWwindow;

//settings are passed as specialisation constans, so they need to be encoded as ints
struct HistogramSettings {
    float minValue;
    float maxValue;
    int maxTileCount;
};

//these enumb values must correspond to the shader values
enum class DiffuseBRDF : int { Lambert = 0, Disney = 1, CoDWWII = 2, Titanfall2 = 3};
enum class DirectSpecularMultiscattering : int { McAuley = 0, Simplified = 1, ScaledGGX = 2, None = 3};
enum class IndirectLightingTech : int { SDFTrace, ConstantAmbient };

struct ShadingConfig {
    DiffuseBRDF diffuseBRDF = DiffuseBRDF::CoDWWII;
    DirectSpecularMultiscattering directMultiscatter = DirectSpecularMultiscattering::McAuley;
	IndirectLightingTech indirectLightingTech = IndirectLightingTech::SDFTrace;
    bool useIndirectMultiscatter = true;
    bool useGeometryAA = true;
	bool indirectLightingHalfRes = true;
};

enum class HistorySamplingTech : int { Bilinear=0, Bicubic16Tap=1, Bicubic9Tap=2, Bicubic5Tap=3, Bicubic1Tap=4 };
enum class SDFDebugMode : int { None=0, VisualizeSDF=1 };

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
	ImageHandle sdf;
};

//simple wrapper to keep all images and framebuffers used in a frame in one place
//simplifies keeping resources of multiples frames around for temporal techniques
//motion buffer is shared between all frames as it's never reused
struct FrameRenderTargets {
    ImageHandle colorBuffer;
    ImageHandle motionBuffer;
    ImageHandle depthBuffer;
    FramebufferHandle colorFramebuffer;
    FramebufferHandle prepassFramebuffer;
};

//texture indices for direct use in shader, index into global texture array
struct Material {
	uint32_t albedoTextureIndex;
	uint32_t normalTextureIndex;
	uint32_t specularTextureIndex;
};

struct MeshFrontend {
	MeshHandle				backendHandle;
	uint32_t				sdfTextureIndex;
	glm::vec3				meanAlbedo;
	Material				material;
	AxisAlignedBoundingBox	localBB;
};

struct SDFInstance {
	glm::vec3 localExtends;
	uint32_t sdfTextureIndex;	//indexes into global texture descriptor array
	glm::vec3 meanAlbedo;
	float padding;
	glm::mat4x4 worldToLocal;
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

    //before call camera settings and such must be set
    //after call drawcalls can be made
    void prepareForDrawcalls();

    void renderScene(const std::vector<RenderObject>& scene);
    void renderFrame();

private:

    void setupGlobalShaderInfoLayout();
	void setupGlobalShaderInfoResources();

    //declare render passes to backend
    //this has to be done before meshes drawcalls can be issued
    void prepareRenderpasses();

    //computes image histogram using compute shaders
    void computeColorBufferHistogram(const ImageHandle lastFrameColor) const;
	void updateSkyLut() const;
    void renderSky(const FramebufferHandle framebuffer, const RenderPassHandle parent) const;
    void renderSunShadowCascades() const;
    void computeExposure() const;
    void renderDepthPrepass(const FramebufferHandle framebuffer) const;
    void computeDepthPyramid(const ImageHandle depthBuffer) const;
    void computeSunLightMatrices() const;
	void diffuseSDFTrace(const FrameRenderTargets& currentTarget) const;
	void filterIndirectDiffuse(const FrameRenderTargets& currentFrame, const FrameRenderTargets& lastFrame) const;
	void downscaleDepth(const FrameRenderTargets& currentTarget) const;
    void renderForwardShading(const std::vector<RenderPassHandle>& externalDependencies, const FramebufferHandle framebuffer) const;
    void copyHDRImage(const ImageHandle src, const ImageHandle dst, RenderPassHandle parent) const; //input must be R11G11B10
    void computeTemporalSuperSampling(const FrameRenderTargets& currentFrame, const FrameRenderTargets& lastFrame,
        const ImageHandle target, const RenderPassHandle parent) const;
    void computeTemporalFilter(const ImageHandle colorSrc, const FrameRenderTargets& currentFrame, const ImageHandle target, const RenderPassHandle parent,
        const ImageHandle historyBufferSrc, const ImageHandle historyBufferDst) const;
    void computeTonemapping(const RenderPassHandle parent, const ImageHandle& src) const;
    void renderDebugGeometry(const FramebufferHandle framebuffer) const;
    void issueSkyDrawcalls();
	void renderSDFVisualization(const ImageHandle targetImage, const RenderPassHandle parent) const;
	void sdfInstanceCulling(const float sdfInfluenceRadius) const;

	void updateSceneSDFInfo(const AxisAlignedBoundingBox& sceneBB);

	void resizeIndirectLightingBuffers();

    //checks a map of all loaded images if it is avaible, returns existing image if possible    
    bool loadImageFromPath(std::filesystem::path path, ImageHandle* outImageHandle);
    std::unordered_map<std::string, ImageHandle> m_textureMap; //using string instead of path to use default string hash

    void computeBRDFLut();

    //diffuse and specular convolution of sky lut for image based lighting
    void skyIBLConvolution();

	std::vector<MeshFrontend> m_frontendMeshes;

    uint32_t m_screenWidth = 800;
    uint32_t m_screenHeight = 600;

    //drawcall stats
    uint32_t m_currentMeshCount = 0;                //mesh commands received
    uint32_t m_currentMainPassDrawcallCount = 0;    //executed after camera culling
    uint32_t m_currentShadowPassDrawcallCount = 0;  //executed after shadow frustum culling

	uint32_t m_currentSDFInstanceCount = 0;

    //timings are cached and not updated every frame to improve readability
    std::vector<RenderPassTime> m_currentRenderTimings;
    float m_renderTimingUpdateFrequency = 0.2f;
    float m_renderTimingTimeSinceLastUpdate = 0.f;

	glm::ivec3 m_sceneSDFResolution = glm::ivec3(1);

    bool m_didResolutionChange = false;
    bool m_minimized = false;
	bool m_renderBoundingBoxes = false;

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
	float m_sdfShadowDistance = 30.f;
    
    void updateCameraFrustum();
    void updateShadowFrustum();

    HistogramSettings createHistogramSettings();

    ShadingConfig m_shadingConfig;
    TemporalFilterSettings m_temporalFilterSettings;
    AtmosphereSettings m_atmosphereSettings;
	SDFDebugMode m_sdfDebugMode = SDFDebugMode::None;

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
    RenderPassHandle m_hdrImageCopyPass;		//input must be R11G11B10
    RenderPassHandle m_colorToLuminancePass;
    RenderPassHandle m_visualizeSDFPass;
	RenderPassHandle m_diffuseSDFTracePass;
	RenderPassHandle m_indirectDiffuseFilterSpatialPass[2];
	RenderPassHandle m_indirectDiffuseFilterTemporalPass;
	RenderPassHandle m_depthDownscalePass;
	RenderPassHandle m_indirectLightingUpscale;
	RenderPassHandle m_sdfCameraFrustumCulling;
	RenderPassHandle m_sdfShadowFrustumCulling;
	RenderPassHandle m_sdfCameraTileCulling;
	RenderPassHandle m_sdfShadowTileCulling;

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
    ImageHandle m_sceneLuminance;
    ImageHandle m_lastFrameLuminance;
	ImageHandle m_indirectDiffuse_Y_SH[2];			//ping pong buffers for filtering, Y component of YCoCg color space as spherical harmonics		
	ImageHandle m_indirectDiffuse_CoCg[2];			//ping pong buffers for filtering, CoCg component of YCoCg color space
	ImageHandle m_indirectDiffuseHistory_Y_SH[2];	//Y component of YCoCg color space as spherical harmonics
	ImageHandle m_indirectDiffuseHistory_CoCg[2];	//CoCg component of YCoCg color space
	ImageHandle m_worldSpaceNormalImage;
	ImageHandle m_depthHalfRes;
	ImageHandle m_indirectLightingFullRes_Y_SH;
	ImageHandle m_indirectLightingFullRes_CoCg;

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
	SamplerHandle m_sampler_nearestRepeat;

    FramebufferHandle	m_shadowCascadeFramebuffers[4];
    FrameRenderTargets	m_frameRenderTargets[2];

    MeshHandle m_skyCube;
    MeshHandle m_quad;
	MeshHandle m_boundingBoxMesh;

    DynamicMeshHandle m_cameraFrustumModel;
    DynamicMeshHandle m_shadowFrustumModel;

    StorageBufferHandle m_histogramPerTileBuffer;
    StorageBufferHandle m_histogramBuffer;
    StorageBufferHandle m_lightBuffer;          //previous exposure and exposured light values
    StorageBufferHandle m_sunShadowInfoBuffer;  //light matrices and cascade splits
    StorageBufferHandle m_depthPyramidSyncBuffer;
	StorageBufferHandle m_mainPassTransformsBuffer;
	StorageBufferHandle m_shadowPassTransformsBuffer;
	StorageBufferHandle m_boundingBoxDebugRenderMatrices;
	StorageBufferHandle m_sdfInstanceBuffer;
	StorageBufferHandle m_sdfCameraFrustumCulledInstances;
	StorageBufferHandle m_sdfShadowFrustumCulledInstances;
	StorageBufferHandle m_sdfInstanceWorldBBBuffer;
	StorageBufferHandle m_sdfCameraCulledTiles;
	StorageBufferHandle m_sdfShadowCulledTiles;

    UniformBufferHandle m_globalUniformBuffer;
    UniformBufferHandle m_atmosphereSettingsBuffer;
    UniformBufferHandle m_taaResolveWeightBuffer;
    UniformBufferHandle m_sdfVolumeInfoBuffer;
	UniformBufferHandle m_cameraFrustumBuffer;
	UniformBufferHandle m_shadowFrustumPointsBuffer;
	UniformBufferHandle m_shadowFrustumInfoBuffer;
	UniformBufferHandle m_sdfTraceInfluenceRangeBuffer;

    GraphicPassShaderDescriptions createForwardPassShaderDescription(const ShadingConfig& config);
    ShaderDescription createBRDFLutShaderDescription(const ShadingConfig& config);
    ShaderDescription createTemporalFilterShaderDescription();
    ShaderDescription createTemporalSupersamplingShaderDescription();
	ShaderDescription createIndirectLightingMipCreationShaderDescription();
	ShaderDescription createEdgeMipCreationShaderDescription();

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
	float m_sdfTraceInfluenceRadius = 3.f;
   
    void drawUi();
};

extern RenderFrontend gRenderFrontend;