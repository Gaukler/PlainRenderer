#include "pch.h"
#include "Volumetrics.h"
#include <Utilities/MathUtils.h>
#include "Runtime/Timer.h"
#include "Runtime/FrameIndex.h"
#include "Common/Noise.h"

const uint32_t scatteringTransmittanceFroxelTileSize = 8;
const uint32_t scatteringTransmittanceDepthSliceCount = 64;

glm::ivec3 computeVolumetricLightingFroxelResolution(const uint32_t screenWidth, const uint32_t screenHeight) {
    return glm::ivec3(
        glm::ceil(screenWidth  / float(scatteringTransmittanceFroxelTileSize)),
        glm::ceil(screenHeight / float(scatteringTransmittanceFroxelTileSize)),
        scatteringTransmittanceDepthSliceCount);
}

void Volumetrics::init(const int screenWidth, const int screenHeight) {

    const glm::ivec3 froxelResolution = computeVolumetricLightingFroxelResolution(screenWidth, screenHeight);

    // scattering/transmittance froxels
    {
        ImageDescription desc;
        desc.width = froxelResolution.x;
        desc.height = froxelResolution.y;
        desc.depth = froxelResolution.z;
        desc.type = ImageType::Type3D;
        desc.format = ImageFormat::RGBA16_sFloat;
        desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_scatteringTransmittanceVolume = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // volumetric lighting integration froxels
    {
        ImageDescription desc;
        desc.width = froxelResolution.x;
        desc.height = froxelResolution.y;
        desc.depth = froxelResolution.z;
        desc.type = ImageType::Type3D;
        desc.format = ImageFormat::RGBA16_sFloat;
        desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_volumetricIntegrationVolume = gRenderBackend.createImage(desc, nullptr, 0);
        m_volumetricLightingHistory[0] = gRenderBackend.createImage(desc, nullptr, 0);
        m_volumetricLightingHistory[1] = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // volume material froxels
    {
        ImageDescription desc;
        desc.width = froxelResolution.x;
        desc.height = froxelResolution.y;
        desc.depth = froxelResolution.z;
        desc.type = ImageType::Type3D;
        desc.format = ImageFormat::RGBA16_sFloat;
        desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_volumeMaterialVolume = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // perlin noise 3D
    {
        const int noiseResolution = 32;

        ImageDescription desc;
        desc.width = noiseResolution;
        desc.height = noiseResolution;
        desc.depth = noiseResolution;
        desc.type = ImageType::Type3D;
        desc.format = ImageFormat::R8;
        desc.usageFlags = ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        const std::vector<uint8_t> perlinNoiseData = generate3DPerlinNoise(glm::ivec3(noiseResolution), 8);
        m_perlinNoise3D = gRenderBackend.createImage(desc, perlinNoiseData.data(), sizeof(uint8_t) * perlinNoiseData.size());
    }
    // volumetric lighting settings
    {
        UniformBufferDescription desc;
        desc.size = sizeof(VolumetricsBufferContents);
        m_volumetricsSettingsUniforms = gRenderBackend.createUniformBuffer(desc);
    }
    // froxel volume material
    {
        ComputePassDescription desc;
        desc.name = "Froxel volume material";
        desc.shaderDescription.srcPathRelative = "froxelVolumeMaterial.comp";
        m_froxelVolumeMaterialPass = gRenderBackend.createComputePass(desc);
    }
    // froxel light scattering
    {
        ComputePassDescription desc;
        desc.name = "Froxel light scattering";
        desc.shaderDescription.srcPathRelative = "froxelLightScattering.comp";
        m_froxelScatteringTransmittancePass = gRenderBackend.createComputePass(desc);
    }
    // froxel light integration
    {
        ComputePassDescription desc;
        desc.name = "Volumetric light integration";
        desc.shaderDescription.srcPathRelative = "volumetricLightingIntegration.comp";
        m_volumetricLightingIntegration = gRenderBackend.createComputePass(desc);
    }
    // volumetric lighting reprojection
    {
        ComputePassDescription desc;
        desc.name = "Volumetric lighting reprojection";
        desc.shaderDescription.srcPathRelative = "volumeLightingReprojection.comp";
        m_volumetricLightingReprojection = gRenderBackend.createComputePass(desc);
    }
}

void Volumetrics::resizeTextures(const int width, const int height) {

    const glm::ivec3 froxelResolution = computeVolumetricLightingFroxelResolution(width, height);
    gRenderBackend.resizeImages({ 
        m_scatteringTransmittanceVolume, 
        m_volumetricIntegrationVolume, 
        m_volumeMaterialVolume,
        m_volumetricLightingHistory[0], 
        m_volumetricLightingHistory[1] },
        froxelResolution.x, 
        froxelResolution.y);
}

RenderPassHandle Volumetrics::computeVolumetricLighting(const VolumetricsSettings& settings, const WindSettings& wind,
    const Dependencies& dependencies) {

    const int jitterIndex = FrameIndex::getFrameIndexMod8();
    m_state.sampleOffset = hammersley2D(jitterIndex).x - 0.5f;
    m_state.windSampleOffset += wind.vector * wind.speed * Timer::getDeltaTimeFloat();

    VolumetricsBufferContents bufferContents;
    bufferContents.state = m_state;
    bufferContents.settings = settings;
    gRenderBackend.setUniformBufferData(m_volumetricsSettingsUniforms,
        (void*)&bufferContents, sizeof(bufferContents));

    const ImageDescription materialFroxelDescription = gRenderBackend.getImageDescription(m_volumeMaterialVolume);

    // setup material volume
    {
        ComputePassExecution exe;
        exe.genericInfo.handle = m_froxelVolumeMaterialPass;
        exe.genericInfo.resources.storageImages = {
            ImageResource(m_volumeMaterialVolume, 0, 0)
        };
        exe.genericInfo.resources.sampledImages = {
            ImageResource(m_perlinNoise3D, 0, 1)
        };
        exe.genericInfo.resources.uniformBuffers = {
            UniformBufferResource(m_volumetricsSettingsUniforms, 2)
        };

        const int groupSize = 4;
        exe.dispatchCount[0] = (uint32_t)glm::ceil(materialFroxelDescription.width  / float(groupSize));
        exe.dispatchCount[1] = (uint32_t)glm::ceil(materialFroxelDescription.height / float(groupSize));
        exe.dispatchCount[2] = (uint32_t)glm::ceil(materialFroxelDescription.depth  / float(groupSize));

        gRenderBackend.setComputePassExecution(exe);
    }
    // scattering/transmittance computation
    {
        ComputePassExecution exe;
        exe.genericInfo.handle = m_froxelScatteringTransmittancePass;
        exe.genericInfo.parents = dependencies.parents;
        exe.genericInfo.parents.push_back(m_froxelVolumeMaterialPass);
        exe.genericInfo.resources.storageImages = {
            ImageResource(m_scatteringTransmittanceVolume, 0, 0)
        };
        exe.genericInfo.resources.sampledImages = {
            ImageResource(dependencies.shadowMap, 0, 1),
            ImageResource(m_volumeMaterialVolume, 0, 2)
        };
        exe.genericInfo.resources.storageBuffers = {
            StorageBufferResource(dependencies.sunShadowInfoBuffer, true, 3),
            StorageBufferResource(dependencies.lightBuffer, true, 4)
        };
        exe.genericInfo.resources.uniformBuffers = {
            UniformBufferResource(m_volumetricsSettingsUniforms, 5)
        };

        const int groupSize = 4;
        exe.dispatchCount[0] = (uint32_t)glm::ceil(materialFroxelDescription.width  / float(groupSize));
        exe.dispatchCount[1] = (uint32_t)glm::ceil(materialFroxelDescription.height / float(groupSize));
        exe.dispatchCount[2] = (uint32_t)glm::ceil(materialFroxelDescription.depth  / float(groupSize));

        gRenderBackend.setComputePassExecution(exe);
    }

    const int frameIndexMod2 = FrameIndex::getFrameIndexMod2();

    const ImageHandle reprojectionTarget = m_volumetricLightingHistory[frameIndexMod2];
    const ImageHandle reprojectionHistory = m_volumetricLightingHistory[(frameIndexMod2 + 1) % 2];

    // temporal reprojection
    {
        ComputePassExecution exe;
        exe.genericInfo.handle = m_volumetricLightingReprojection;
        exe.genericInfo.parents = { m_froxelScatteringTransmittancePass };
        exe.genericInfo.resources.storageImages = {
            ImageResource(reprojectionTarget, 0, 0)
        };
        exe.genericInfo.resources.sampledImages = {
            ImageResource(m_scatteringTransmittanceVolume, 0, 1),
            ImageResource(reprojectionHistory, 0, 2)
        };
        exe.genericInfo.resources.uniformBuffers = {
            UniformBufferResource(m_volumetricsSettingsUniforms, 3)
        };

        const int groupSize = 4;
        exe.dispatchCount[0] = (uint32_t)glm::ceil(materialFroxelDescription.width  / float(groupSize));
        exe.dispatchCount[1] = (uint32_t)glm::ceil(materialFroxelDescription.height / float(groupSize));
        exe.dispatchCount[2] = (uint32_t)glm::ceil(materialFroxelDescription.depth  / float(groupSize));

        gRenderBackend.setComputePassExecution(exe);
    }
    // integration
    {
        ComputePassExecution exe;
        exe.genericInfo.handle = m_volumetricLightingIntegration;
        exe.genericInfo.parents = { m_volumetricLightingReprojection };
        exe.genericInfo.resources.storageImages = {
            ImageResource(m_volumetricIntegrationVolume, 0, 0)
        };
        exe.genericInfo.resources.sampledImages = {
            ImageResource(reprojectionTarget, 0, 1)
        };
        exe.genericInfo.resources.uniformBuffers = {
            UniformBufferResource(m_volumetricsSettingsUniforms, 2)
        };

        const int groupSize = 8;
        exe.dispatchCount[0] = (uint32_t)glm::ceil(materialFroxelDescription.width  / float(groupSize));
        exe.dispatchCount[1] = (uint32_t)glm::ceil(materialFroxelDescription.height / float(groupSize));
        exe.dispatchCount[2] = 1;

        gRenderBackend.setComputePassExecution(exe);
    }
    return m_volumetricLightingIntegration;
}

ImageHandle Volumetrics::getIntegrationVolume() const {
    return m_volumetricIntegrationVolume;
}

UniformBufferHandle Volumetrics::getVolumetricsInfoBuffer() const {
    return m_volumetricsSettingsUniforms;
}