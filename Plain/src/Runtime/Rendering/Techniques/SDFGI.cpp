#include "pch.h"
#include "SDFGI.h"
#include "Common/VolumeInfo.h"
#include "Common/Utilities/GeneralUtils.h"
#include "Runtime/Rendering/SceneConfig.h"
#include "Runtime/FrameIndex.h"
#include "Runtime/RuntimeScene.h"
#include "Common/sdfUtilities.h"

const size_t sdfCameraCullingTileSize = 32;
const size_t maxSdfObjectsPerTile = 100;    // must be the same as shader constant maxObjectsPerTile in sdfCulling.inc

ShaderDescription createSDFDebugShaderDescription(const SDFDebugSettings& settings, const int sunShadowCascadeIndex) {
    ShaderDescription desc;
    desc.srcPathRelative = "sdfDebugVisualisation.comp";
    // visualisation mode
    const SDFVisualisationMode& visualisationMode = settings.visualisationMode;
    desc.specialisationConstants.push_back({
        0,                                                                      // location
        dataToCharArray((void*)&visualisationMode, sizeof(visualisationMode))   // value
        });
    // shadow cascade index
    desc.specialisationConstants.push_back({
        1,                                                                              // location
        dataToCharArray((void*)&sunShadowCascadeIndex, sizeof(sunShadowCascadeIndex))   // value
        });
    return desc;
}

ShaderDescription createSDFDiffuseTraceShaderDescription(const SDFTraceSettings& settings, 
    const int sunShadowCascadeIndex) {

    ShaderDescription desc;
    desc.srcPathRelative = "sdfDiffuseTrace.comp";
    // strict influence radius cutoff
    desc.specialisationConstants.push_back({
        0,                                                                                                          // location
        dataToCharArray((void*)&settings.strictInfluenceRadiusCutoff, sizeof(settings.strictInfluenceRadiusCutoff)) // value
        });
    // shadow cascade index
    desc.specialisationConstants.push_back({
        1,                                                                              // location
        dataToCharArray((void*)&sunShadowCascadeIndex, sizeof(sunShadowCascadeIndex))   // value
        });
    return desc;
}

void SDFGI::init(const glm::ivec2 screenResolution, const SDFTraceSettings& traceSettings, 
    const SDFDebugSettings& debugSettings, const int sunShadowCascadeIndex) {

    // indirect diffuse Y component spherical harmonics
    {
        ImageDescription desc;
        desc.width  = traceSettings.halfResTrace ? screenResolution.x / 2 : screenResolution.x;
        desc.height = traceSettings.halfResTrace ? screenResolution.y / 2 : screenResolution.y;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::RGBA16_sFloat;
        desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_indirectDiffuseHistory_Y_SH[0] = gRenderBackend.createImage(desc, nullptr, 0);
        m_indirectDiffuseHistory_Y_SH[1] = gRenderBackend.createImage(desc, nullptr, 0);

        m_indirectDiffuse_Y_SH[0] = gRenderBackend.createImage(desc, nullptr, 0);
        m_indirectDiffuse_Y_SH[1] = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // indirect diffuse CoCg component
    {
        ImageDescription desc;
        desc.width  = traceSettings.halfResTrace ? screenResolution.x / 2 : screenResolution.x;
        desc.height = traceSettings.halfResTrace ? screenResolution.y / 2 : screenResolution.y;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::RG16_sFloat;
        desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_indirectDiffuse_CoCg[0] = gRenderBackend.createImage(desc, nullptr, 0);
        m_indirectDiffuse_CoCg[1] = gRenderBackend.createImage(desc, nullptr, 0);
        m_indirectDiffuseHistory_CoCg[0] = gRenderBackend.createImage(desc, nullptr, 0);
        m_indirectDiffuseHistory_CoCg[1] = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // indirect lighting full res Y component spherical harmonics
    {
        ImageDescription desc;
        desc.width  = screenResolution.x;
        desc.height = screenResolution.y;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::RGBA16_sFloat;
        desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_indirectLightingFullRes_Y_SH = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // indirect lighting full res CoCg component
    {
        ImageDescription desc;
        desc.width  = screenResolution.x;
        desc.height = screenResolution.y;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::RG16_sFloat;
        desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_indirectLightingFullRes_CoCg = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // sdf instance buffer
    {
        StorageBufferDescription desc;
        desc.size = maxObjectCountMainScene * sizeof(SDFInstance) + sizeof(uint32_t) * 4; // extra uint for object count with padding to 16 byte
        m_sdfInstanceBuffer = gRenderBackend.createStorageBuffer(desc);
    }
    // sdf camera frustum culled instances
    {
        StorageBufferDescription desc;
        desc.size = maxObjectCountMainScene * sizeof(uint32_t) + sizeof(uint32_t);
        m_sdfCameraFrustumCulledInstances = gRenderBackend.createStorageBuffer(desc);
    }
    // camera frustum buffer
    {
        UniformBufferDescription desc;
        desc.size = 12 * sizeof(glm::vec4);
        m_cameraFrustumBuffer = gRenderBackend.createUniformBuffer(desc);
    }
    // sdf instance world bounding boxes
    {
        StorageBufferDescription desc;
        desc.size = maxObjectCountMainScene * 2 * sizeof(glm::vec4);
        m_sdfInstanceWorldBBBuffer = gRenderBackend.createStorageBuffer(desc);
    }
    // sdf camera culled tiles
    {
        StorageBufferDescription desc;
        // FIXME: handle bigger resolutions and don't allocate for worst case
        const size_t maxWidth = 1920;
        const size_t maxHeight = 1080;
        const size_t tileCount = (size_t)glm::ceil(maxWidth / sdfCameraCullingTileSize) * (size_t)glm::ceil(maxHeight / sdfCameraCullingTileSize);
        const size_t tileSize = maxSdfObjectsPerTile * sizeof(uint32_t) + sizeof(uint32_t); //one uint index per object + object count
        desc.size = tileCount * tileSize;
        m_sdfCameraCulledTiles = gRenderBackend.createStorageBuffer(desc);
    }
    // sdf trace influence range buffer
    {
        UniformBufferDescription desc;
        desc.size = sizeof(float);
        m_sdfTraceInfluenceRangeBuffer = gRenderBackend.createUniformBuffer(desc);
    }
    // sdf debug pass
    {
        ComputePassDescription desc;
        desc.name = "Visualize SDF";
        desc.shaderDescription = createSDFDebugShaderDescription(debugSettings, sunShadowCascadeIndex);
        m_sdfDebugVisualisationPass = gRenderBackend.createComputePass(desc);
    }
    // sdf indirect lighting
    {
        ComputePassDescription desc;
        desc.name = "Indirect diffuse SDF trace";
        desc.shaderDescription = createSDFDiffuseTraceShaderDescription(traceSettings, sunShadowCascadeIndex);
        m_diffuseSDFTracePass = gRenderBackend.createComputePass(desc);
    }
    // indirect diffuse spatial filter
    {
        for (int i = 0; i < 2; i++) {
            ComputePassDescription desc;
            desc.name = "Indirect diffuse spatial filter";
            desc.shaderDescription.srcPathRelative = "filterIndirectDiffuseSpatial.comp";

            // index constant
            desc.shaderDescription.specialisationConstants.push_back({
            0,                                      // location
            dataToCharArray((void*)&i, sizeof(i))   // value
                });
            m_indirectDiffuseFilterSpatialPass[i] = gRenderBackend.createComputePass(desc);
        }
    }
    // indirect diffuse temporal filter
    {
        ComputePassDescription desc;
        desc.name = "Indirect diffuse temporal filter";
        desc.shaderDescription.srcPathRelative = "filterIndirectDiffuseTemporal.comp";
        m_indirectDiffuseFilterTemporalPass = gRenderBackend.createComputePass(desc);
    }
    // indirect lighting upscale
    {
        ComputePassDescription desc;
        desc.name = "Indirect lighting upscale";
        desc.shaderDescription.srcPathRelative = "indirectLightUpscale.comp";
        m_indirectLightingUpscale = gRenderBackend.createComputePass(desc);
    }
    // sdf instance frustum culling
    {
        ComputePassDescription desc;
        desc.name = "SDF camera frustum culling";
        desc.shaderDescription.srcPathRelative = "sdfCameraFrustumCulling.comp";
        m_sdfCameraFrustumCulling = gRenderBackend.createComputePass(desc);
    }
    // sdf camera tile culling
    {
        ComputePassDescription desc;
        desc.name = "SDF camera tile culling";
        desc.shaderDescription.srcPathRelative = "sdfCameraTileCulling.comp";

        // hi-z disabled
        bool useHiZ = false;
        desc.shaderDescription.specialisationConstants = {
        {
            0,                                              // location
            dataToCharArray((void*)&useHiZ, sizeof(useHiZ)) // value
        }
        };
        m_sdfCameraTileCulling = gRenderBackend.createComputePass(desc);

        // hi-z enabled
        useHiZ = true;
        desc.shaderDescription.specialisationConstants = {
            {
                0,                                              // location
                dataToCharArray((void*)&useHiZ, sizeof(useHiZ)) // value
            }
        };
        m_sdfCameraTileCullingHiZ = gRenderBackend.createComputePass(desc);
    }
}

void SDFGI::resize(const int width, const int height, const SDFTraceSettings& settings) {
    gRenderBackend.resizeImages({
        m_indirectLightingFullRes_Y_SH,
        m_indirectLightingFullRes_CoCg
        },
        width,
        height);

    const uint32_t divider = settings.halfResTrace ? 2 : 1;
    gRenderBackend.resizeImages({
        m_indirectDiffuse_Y_SH[0],
        m_indirectDiffuse_Y_SH[1],
        m_indirectDiffuse_CoCg[0],
        m_indirectDiffuse_CoCg[1],
        m_indirectDiffuseHistory_Y_SH[0],
        m_indirectDiffuseHistory_Y_SH[1],
        m_indirectDiffuseHistory_CoCg[0],
        m_indirectDiffuseHistory_CoCg[1]
        }, 
        width  / divider,
        height / divider);
}

void SDFGI::updateSDFScene(const std::vector<RenderObject>& scene, const std::vector<MeshFrontend>& frontendMeshes) {

    // TODO: instead of updating complete scene transforms every frame, track dirty transforms and ony write changes using compute shader

    struct GPUBoundingBox {
        glm::vec3 min = glm::vec3(0);
        float padding1 = 0.f;
        glm::vec3 max = glm::vec3(0);
        float padding2 = 0.f;
    };
    std::vector<GPUBoundingBox> instanceWorldBBs;
    std::vector<SDFInstance> instanceData;
    instanceData.reserve(scene.size());
    instanceWorldBBs.reserve(scene.size());
    for (const RenderObject& obj : scene) {

        const MeshFrontend& mesh = frontendMeshes[obj.mesh.index];
        // skip meshes without sdf
        if (mesh.sdfTextureIndex < 0) {
            continue;
        }

        // culling requires the padded bounding box, used for rendering and computation
        const AxisAlignedBoundingBox paddedWorldBB = padSDFBoundingBox(obj.bbWorld);
        GPUBoundingBox worldBB;
        worldBB.min = paddedWorldBB.min;
        worldBB.max = paddedWorldBB.max;
        instanceWorldBBs.push_back(worldBB);

        SDFInstance instance;
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
    m_sdfInstanceCount = (uint32_t)instanceData.size();
    uint32_t padding[3] = { 0, 0, 0 };
    memcpy(bufferData.data(), &m_sdfInstanceCount, sizeof(uint32_t));
    memcpy(bufferData.data() + sizeof(uint32_t), &padding, sizeof(uint32_t) * 3);
    memcpy(bufferData.data() + sizeof(uint32_t) * 4, instanceData.data(), sizeof(SDFInstance) * instanceData.size());
    gRenderBackend.setStorageBufferData(m_sdfInstanceBuffer, bufferData.data(), bufferData.size());

    gRenderBackend.setStorageBufferData(m_sdfInstanceWorldBBBuffer, instanceWorldBBs.data(),
        instanceWorldBBs.size() * sizeof(GPUBoundingBox));
}

SDFGI::IndirectLightingImages SDFGI::getIndirectLightingResults(const bool tracedHalfRes) const{
    IndirectLightingImages result;
    if (tracedHalfRes) {
        result.Y_SH = m_indirectLightingFullRes_Y_SH;
        result.CoCg = m_indirectLightingFullRes_CoCg;
    }
    else {
        result.Y_SH = m_indirectDiffuseHistory_Y_SH[0];
        result.CoCg = m_indirectDiffuseHistory_CoCg[0];
    }
    return result;
}

RenderPassHandle SDFGI::computeIndirectLighting(const SDFTraceDependencies& dependencies, const SDFTraceSettings& traceSettings) const {

    diffuseSDFTrace(dependencies, traceSettings);
    const RenderPassHandle filterPass = filterIndirectDiffuse(dependencies, traceSettings);

    return filterPass;
}

RenderPassHandle SDFGI::renderSDFVisualization(const ImageHandle target, const SDFTraceDependencies dependencies,
    const SDFDebugSettings& debugSettings, const SDFTraceSettings& traceSettings) const {

    const float sdfIncluenceRadius = debugSettings.useInfluenceRadiusForDebug ? traceSettings.traceInfluenceRadius : 0.f;

    const ImageDescription targetDescription = gRenderBackend.getImageDescription(m_indirectLightingFullRes_CoCg);
    const glm::ivec2 targetResolution = glm::ivec2(targetDescription.width, targetDescription.height);

    bool useHiZCulling = false;
    if (debugSettings.visualisationMode == SDFVisualisationMode::CameraTileUsage && debugSettings.showCameraTileUsageWithHiZ) {
        useHiZCulling = true;
    }

    const RenderPassHandle cullingPass = sdfInstanceCulling(dependencies, targetResolution, 
        sdfIncluenceRadius, useHiZCulling);

    ComputePassExecution exe;
    exe.genericInfo.handle = m_sdfDebugVisualisationPass;
    exe.genericInfo.resources.storageImages = { ImageResource(target, 0, 0) };
    exe.genericInfo.resources.sampledImages = {
        ImageResource(dependencies.skyLut, 0, 2),
        ImageResource(dependencies.shadowMap, 0, 7)
    };
    exe.genericInfo.resources.storageBuffers = {
        StorageBufferResource(dependencies.lightBuffer, true, 1),
        StorageBufferResource(m_sdfInstanceBuffer, true, 3),
        StorageBufferResource(m_sdfCameraCulledTiles, true, 4),
        StorageBufferResource(m_sdfCameraFrustumCulledInstances, true, 5),
        StorageBufferResource(dependencies.sunShadowInfoBuffer, true, 6)
    };

    exe.dispatchCount[0] = (uint32_t)std::ceil(targetResolution.x  / 8.f);
    exe.dispatchCount[1] = (uint32_t)std::ceil(targetResolution.y / 8.f);
    exe.dispatchCount[2] = 1;
    exe.genericInfo.parents = { cullingPass };

    gRenderBackend.setComputePassExecution(exe);

    return m_sdfDebugVisualisationPass;
}

void SDFGI::updateSDFDebugSettings(const SDFDebugSettings& settings, const int sunShadowCascadeIndex) {
    gRenderBackend.updateComputePassShaderDescription(m_sdfDebugVisualisationPass, createSDFDebugShaderDescription(settings, sunShadowCascadeIndex));
}

void SDFGI::updateSDFTraceSettings(const SDFTraceSettings& settings, const int sunShadowCascadeIndex) {
    gRenderBackend.updateComputePassShaderDescription(m_diffuseSDFTracePass,
        createSDFDiffuseTraceShaderDescription(settings, sunShadowCascadeIndex));
}

RenderPassHandle SDFGI::diffuseSDFTrace(const SDFTraceDependencies& dependencies, const SDFTraceSettings& traceSettings) const {

    const ImageDescription targetDescription = gRenderBackend.getImageDescription(m_indirectDiffuse_CoCg[0]);
    const glm::ivec2 targetResolution = glm::ivec2(targetDescription.width, targetDescription.height);

    const RenderPassHandle cullingPass = sdfInstanceCulling(dependencies, targetResolution, traceSettings.traceInfluenceRadius, true);

    ComputePassExecution exe;
    exe.genericInfo.handle = m_diffuseSDFTracePass;
    exe.genericInfo.parents = { cullingPass };
    for (const auto& pass : dependencies.parents) {
        exe.genericInfo.parents.push_back(pass);
    }

    exe.genericInfo.resources.storageImages = {
        ImageResource(m_indirectDiffuse_Y_SH[0], 0, 0),
        ImageResource(m_indirectDiffuse_CoCg[0], 0, 1)
    };

    exe.genericInfo.resources.sampledImages = {
        ImageResource(dependencies.currentFrame.depthBuffer, 0, 2),
        ImageResource(dependencies.worldSpaceNormals, 0, 3),
        ImageResource(dependencies.skyLut, 0, 4),
        ImageResource(dependencies.shadowMap, 0, 10)
    };
    exe.genericInfo.resources.storageBuffers = {
        StorageBufferResource(dependencies.lightBuffer, true, 5),
        StorageBufferResource(m_sdfInstanceBuffer, true, 6),
        StorageBufferResource(m_sdfCameraCulledTiles, true, 7),
        StorageBufferResource(dependencies.sunShadowInfoBuffer, true, 9)
    };

    exe.genericInfo.resources.uniformBuffers = {
        UniformBufferResource(m_sdfTraceInfluenceRangeBuffer, 8)
    };

    const float localThreadSize = 8.f;

    exe.dispatchCount[0] = glm::ceil(targetResolution.x / localThreadSize);
    exe.dispatchCount[1] = glm::ceil(targetResolution.y / localThreadSize);
    exe.dispatchCount[2] = 1;

    gRenderBackend.setComputePassExecution(exe);

    return m_diffuseSDFTracePass;
}

RenderPassHandle SDFGI::filterIndirectDiffuse(const SDFTraceDependencies& dependencies, const SDFTraceSettings& traceSettings) const {

    const ImageHandle depthSrc = traceSettings.halfResTrace ? dependencies.depthHalfRes : dependencies.currentFrame.depthBuffer;

    const ImageDescription targetDescription = gRenderBackend.getImageDescription(m_indirectDiffuse_Y_SH[1]);
    const glm::vec2 workingResolution = glm::ivec2(targetDescription.width, targetDescription.height);

    // spatial filter on input
    {
        ComputePassExecution exe;
        exe.genericInfo.handle = m_indirectDiffuseFilterSpatialPass[0];
        exe.genericInfo.parents = { m_diffuseSDFTracePass };

        exe.genericInfo.resources.storageImages = {
            ImageResource(m_indirectDiffuse_Y_SH[1], 0, 0),
            ImageResource(m_indirectDiffuse_CoCg[1], 0, 1)
        };
        exe.genericInfo.resources.sampledImages = {
            ImageResource(m_indirectDiffuse_Y_SH[0], 0, 2),
            ImageResource(m_indirectDiffuse_CoCg[0], 0, 3),
            ImageResource(depthSrc, 0, 4),
            ImageResource(dependencies.worldSpaceNormals, 0, 5),
        };

        const float localThreadSize = 8.f;
        const glm::ivec2 dispatchCount = glm::ivec2(glm::ceil(workingResolution / localThreadSize));
        exe.dispatchCount[0] = dispatchCount.x;
        exe.dispatchCount[1] = dispatchCount.y;
        exe.dispatchCount[2] = 1;

        gRenderBackend.setComputePassExecution(exe);
    }
    // temporal filter
    {
        ComputePassExecution exe;
        exe.genericInfo.handle = m_indirectDiffuseFilterTemporalPass;
        exe.genericInfo.parents = { m_indirectDiffuseFilterSpatialPass[0] };

        const uint32_t historySrcIndex = FrameIndex::getFrameIndexMod2();
        const uint32_t historyDstIndex = historySrcIndex == 0 ? 1 : 0;

        exe.genericInfo.resources.storageImages = {
            ImageResource(m_indirectDiffuse_Y_SH[0], 0, 0),
            ImageResource(m_indirectDiffuse_CoCg[0], 0, 1),
            ImageResource(m_indirectDiffuseHistory_Y_SH[1], 0, 2),
            ImageResource(m_indirectDiffuseHistory_CoCg[1], 0, 3)
        };
        exe.genericInfo.resources.sampledImages = {
            ImageResource(m_indirectDiffuse_Y_SH[1], 0, 4),
            ImageResource(m_indirectDiffuse_CoCg[1], 0, 5),
            ImageResource(m_indirectDiffuseHistory_Y_SH[0], 0, 6),
            ImageResource(m_indirectDiffuseHistory_CoCg[0], 0, 7),
            ImageResource(dependencies.currentFrame.motionBuffer, 0, 8),
            ImageResource(dependencies.previousFrame.motionBuffer, 0, 9)
        };

        const float localThreadSize = 8.f;
        const glm::ivec2 dispatchCount = glm::ivec2(glm::ceil(workingResolution / localThreadSize));
        exe.dispatchCount[0] = dispatchCount.x;
        exe.dispatchCount[1] = dispatchCount.y;
        exe.dispatchCount[2] = 1;

        gRenderBackend.setComputePassExecution(exe);
    }
    // spatial filter on history
    {
        ComputePassExecution exe;
        exe.genericInfo.handle = m_indirectDiffuseFilterSpatialPass[1];
        exe.genericInfo.parents = { m_indirectDiffuseFilterTemporalPass };

        exe.genericInfo.resources.storageImages = {
            ImageResource(m_indirectDiffuseHistory_Y_SH[0], 0, 0),
            ImageResource(m_indirectDiffuseHistory_CoCg[0], 0, 1)
        };
        exe.genericInfo.resources.sampledImages = {
            ImageResource(m_indirectDiffuseHistory_Y_SH[1], 0, 2),
            ImageResource(m_indirectDiffuseHistory_CoCg[1], 0, 3),
            ImageResource(depthSrc, 0, 4),
            ImageResource(dependencies.worldSpaceNormals, 0, 5),
        };

        const float localThreadSize = 8.f;
        const glm::ivec2 dispatchCount = glm::ivec2(glm::ceil(workingResolution / localThreadSize));
        exe.dispatchCount[0] = dispatchCount.x;
        exe.dispatchCount[1] = dispatchCount.y;
        exe.dispatchCount[2] = 1;

        gRenderBackend.setComputePassExecution(exe);
    }
    
    RenderPassHandle lastPass = m_indirectDiffuseFilterSpatialPass[1];

    // upscale
    if (traceSettings.halfResTrace) {
        ComputePassExecution exe;
        exe.genericInfo.handle = m_indirectLightingUpscale;
        exe.genericInfo.parents = { m_indirectDiffuseFilterSpatialPass[1] };

        exe.genericInfo.resources.storageImages = {
            ImageResource(m_indirectLightingFullRes_Y_SH, 0, 0),
            ImageResource(m_indirectLightingFullRes_CoCg, 0, 1),
        };
        exe.genericInfo.resources.sampledImages = {
            ImageResource(m_indirectDiffuseHistory_Y_SH[0], 0, 2),
            ImageResource(m_indirectDiffuseHistory_CoCg[0], 0, 3),
            ImageResource(dependencies.currentFrame.depthBuffer, 0, 4),
            ImageResource(dependencies.depthHalfRes, 0, 5)
        };

        const ImageDescription targetImageDescription = gRenderBackend.getImageDescription(m_indirectLightingFullRes_Y_SH);

        const float localThreadSize = 8.f;
        exe.dispatchCount[0] = (uint32_t)glm::ceil(targetImageDescription.width / localThreadSize);
        exe.dispatchCount[1] = (uint32_t)glm::ceil(targetImageDescription.height / localThreadSize);
        exe.dispatchCount[2] = 1;

        gRenderBackend.setComputePassExecution(exe);
        lastPass = m_indirectLightingUpscale;
    }
    return lastPass;
}

RenderPassHandle SDFGI::sdfInstanceCulling(const SDFTraceDependencies& dependencies, 
    const glm::ivec2 targetResolution, const float influenceRadius, const bool hiZCulling) const {

    // camera frustum culling
    {
        struct GPUFrustumData {
            glm::vec4 points[6];
            glm::vec4 normal[6];
        };
        const ViewFrustum& cameraFrustum = dependencies.cameraFrustum;
        GPUFrustumData frustumData;
        // top
        frustumData.points[0] = glm::vec4(cameraFrustum.points.l_u_f, 0);
        frustumData.normal[0] = glm::vec4(cameraFrustum.normals.top, 0);
        // bot
        frustumData.points[1] = glm::vec4(cameraFrustum.points.l_l_f, 0);
        frustumData.normal[1] = glm::vec4(cameraFrustum.normals.bot, 0);
        // near
        frustumData.points[2] = glm::vec4(cameraFrustum.points.l_l_n, 0);
        frustumData.normal[2] = glm::vec4(cameraFrustum.normals.near, 0);
        // far
        frustumData.points[3] = glm::vec4(cameraFrustum.points.l_l_f, 0);
        frustumData.normal[3] = glm::vec4(cameraFrustum.normals.far, 0);
        // left
        frustumData.points[4] = glm::vec4(cameraFrustum.points.l_l_f, 0);
        frustumData.normal[4] = glm::vec4(cameraFrustum.normals.left, 0);
        // right
        frustumData.points[5] = glm::vec4(cameraFrustum.points.r_l_f, 0);
        frustumData.normal[5] = glm::vec4(cameraFrustum.normals.right, 0);

        gRenderBackend.setUniformBufferData(m_cameraFrustumBuffer, &frustumData, sizeof(frustumData));

        // reset culled counter
        uint32_t zero = 0;
        gRenderBackend.setStorageBufferData(m_sdfCameraFrustumCulledInstances, &zero, sizeof(zero));

        ComputePassExecution exe;
        exe.genericInfo.parents = dependencies.parents;
        exe.genericInfo.handle = m_sdfCameraFrustumCulling;
        exe.genericInfo.resources.storageBuffers = {
            StorageBufferResource(m_sdfInstanceBuffer, true, 0),
            StorageBufferResource(m_sdfCameraFrustumCulledInstances, false, 2),
            StorageBufferResource(m_sdfInstanceWorldBBBuffer, true, 3)
        };
        exe.genericInfo.resources.uniformBuffers = {
            UniformBufferResource(m_cameraFrustumBuffer, 1),
            UniformBufferResource(m_sdfTraceInfluenceRangeBuffer, 4)
        };
        exe.dispatchCount[0] = uint32_t(glm::ceil(m_sdfInstanceCount / 64.f));
        exe.dispatchCount[1] = 1;
        exe.dispatchCount[2] = 1;

        gRenderBackend.setComputePassExecution(exe);
    }
    // camera tile culling
    const RenderPassHandle cameraTileCullingPass = hiZCulling ? m_sdfCameraTileCullingHiZ : m_sdfCameraTileCulling;
    {
        ComputePassExecution exe;
        exe.genericInfo.handle = cameraTileCullingPass;
        exe.genericInfo.parents = { m_sdfCameraFrustumCulling };

        const glm::uvec2 tileCount = glm::uvec2(
            glm::ceil(targetResolution.x / float(sdfCameraCullingTileSize)),
            glm::ceil(targetResolution.y / float(sdfCameraCullingTileSize)));

        const uint32_t localGroupSize = 8;

        exe.dispatchCount[0] = uint32_t(glm::ceil(tileCount.x / float(localGroupSize)));
        exe.dispatchCount[1] = uint32_t(glm::ceil(tileCount.y / float(localGroupSize)));
        exe.dispatchCount[2] = 1;

        exe.pushConstants = dataToCharArray((void*)&tileCount, sizeof(tileCount));

        exe.genericInfo.resources.storageBuffers = {
            StorageBufferResource(m_sdfCameraFrustumCulledInstances, true, 0),
            StorageBufferResource(m_sdfInstanceWorldBBBuffer, true, 1),
            StorageBufferResource(m_sdfCameraCulledTiles, false, 2)
        };

        gRenderBackend.setUniformBufferData(m_sdfTraceInfluenceRangeBuffer, &influenceRadius, sizeof(influenceRadius));

        exe.genericInfo.resources.uniformBuffers = {
            UniformBufferResource(m_sdfTraceInfluenceRangeBuffer, 3)
        };

        // -1 because pyramid is already half of screen resolution at base mip
        const int depthPyramidMipLevel = (int)glm::log2(glm::ceil(float(sdfCameraCullingTileSize))) - 1;
        assert(depthPyramidMipLevel == 4);
        exe.genericInfo.resources.sampledImages = {
            ImageResource(dependencies.depthMinMaxPyramid, depthPyramidMipLevel, 4)
        };

        gRenderBackend.setComputePassExecution(exe);
    }
    return cameraTileCullingPass;
}