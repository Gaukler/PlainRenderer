#pragma once
#include "pch.h"
#include "Runtime/Rendering/Backend/RenderBackend.h"
#include "Runtime/Rendering/FrameRenderTargets.h"
#include "Runtime/Rendering/ViewFrustum.h"
#include "Runtime/Rendering/MeshFrontend.h"
#include "Runtime/RuntimeScene.h"

enum class SDFVisualisationMode : int { None = 0, VisualizeSDF = 1, CameraTileUsage = 2, SDFNormals = 3, RaymarchingSteps = 4 };

struct SDFDebugSettings {
    SDFVisualisationMode visualisationMode = SDFVisualisationMode::None;
    bool showCameraTileUsageWithHiZ = true;
    bool useInfluenceRadiusForDebug = false;    // less efficient, but tile usage is same as for indirect light tracing
};

struct SDFTraceSettings {

    bool halfResTrace = true;

    // reject trace hits outside of influence radius
    // loses range, but results outside of influence radius are not entirely accurate, as objects start to be culled
    bool strictInfluenceRadiusCutoff = true;
    // radius in which objects are not culled, increases effect range and computation time
    float traceInfluenceRadius = 5.f;
    // highest sun shadow cascade used for shadowing trace hits
    // if strict influence radius cutoff is disabled hits can be outside influence radius, so extra padding is necessary
    float additionalSunShadowMapPadding = 3.f;
};

struct SDFInstance {
    glm::vec3 localExtends;
    uint32_t sdfTextureIndex;   // indexes into global texture descriptor array
    glm::vec3 meanAlbedo;
    float padding;
    glm::mat4x4 worldToLocal;
};

struct SDFTraceDependencies {
    FrameRenderTargets currentFrame;
    FrameRenderTargets previousFrame;
    std::vector<RenderPassHandle> parents;
    ViewFrustum cameraFrustum;
    ImageHandle depthHalfRes;   // only used if tracing at half resolution
    ImageHandle worldSpaceNormals;
    ImageHandle skyLut;
    ImageHandle shadowMap;
    StorageBufferHandle lightBuffer;
    StorageBufferHandle sunShadowInfoBuffer;
    ImageHandle depthMinMaxPyramid;
};

class SDFGI {
public:
    void init(const glm::ivec2 screenResolution, const SDFTraceSettings& traceSettings, 
        const SDFDebugSettings& debugSettings, const int sunShadowCascadeIndex);

    void resize(const int width, const int height, const SDFTraceSettings& settings);

    void updateSDFScene(const std::vector<RenderObject>& scene, const std::vector<MeshFrontend>& frontendMeshes);

    struct IndirectLightingImages {
        ImageHandle Y_SH;
        ImageHandle CoCg;
    };

    IndirectLightingImages getIndirectLightingResults(const bool tracedHalfRes) const;

    RenderPassHandle computeIndirectLighting(const SDFTraceDependencies& dependencies, const SDFTraceSettings& traceSettings) const;

    RenderPassHandle renderSDFVisualization(const ImageHandle target, const SDFTraceDependencies dependencies,
        const SDFDebugSettings& debugSettings, const SDFTraceSettings& traceSettings) const;

    void updateSDFDebugSettings(const SDFDebugSettings& settings, const int sunShadowCascadeIndex);
    void updateSDFTraceSettings(const SDFTraceSettings& settings, const int sunShadowCascadeIndex);

private:

    // returns list of passes that must be used as parent to wait for results
    // when culling for direct visualisation hi-z culling results in artifacts
    // enable for indirect, disable for direct
    RenderPassHandle sdfInstanceCulling(const SDFTraceDependencies& dependencies, const glm::ivec2 targetResolution, 
        const float influenceRadius, const bool hiZCulling) const;

    RenderPassHandle diffuseSDFTrace(const SDFTraceDependencies& dependencies, const SDFTraceSettings& traceSettings) const;

    RenderPassHandle filterIndirectDiffuse(const SDFTraceDependencies& dependencies, const SDFTraceSettings& traceSettings) const;

    uint32_t m_sdfInstanceCount = 0;

    RenderPassHandle m_diffuseSDFTracePass;
    RenderPassHandle m_indirectDiffuseFilterSpatialPass[2];
    RenderPassHandle m_indirectDiffuseFilterTemporalPass;
    RenderPassHandle m_indirectLightingUpscale;
    RenderPassHandle m_sdfCameraFrustumCulling;
    RenderPassHandle m_sdfCameraTileCulling;
    RenderPassHandle m_sdfCameraTileCullingHiZ;
    RenderPassHandle m_sdfDebugVisualisationPass;

    ImageHandle m_indirectDiffuse_Y_SH[2];          // ping pong buffers for filtering, Y component of YCoCg color space as spherical harmonics		
    ImageHandle m_indirectDiffuse_CoCg[2];          // ping pong buffers for filtering, CoCg component of YCoCg color space
    ImageHandle m_indirectDiffuseHistory_Y_SH[2];   // Y component of YCoCg color space as spherical harmonics
    ImageHandle m_indirectDiffuseHistory_CoCg[2];   // CoCg component of YCoCg color space
    ImageHandle m_indirectLightingFullRes_Y_SH;
    ImageHandle m_indirectLightingFullRes_CoCg;

    StorageBufferHandle m_sdfInstanceBuffer;
    StorageBufferHandle m_sdfCameraFrustumCulledInstances;
    StorageBufferHandle m_sdfInstanceWorldBBBuffer;
    StorageBufferHandle m_sdfCameraCulledTiles;

    UniformBufferHandle m_cameraFrustumBuffer;
    UniformBufferHandle m_sdfTraceInfluenceRangeBuffer;
};