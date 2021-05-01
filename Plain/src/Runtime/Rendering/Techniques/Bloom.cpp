#include "pch.h"
#include "Bloom.h"
#include "Utilities/GeneralUtils.h"
#include "Utilities/MathUtils.h"

const int bloomMipCount = 6;

void Bloom::init(const int textureWidth, const int textureHeight) {
    // bloom downsample
    for (int i = 0; i < bloomMipCount - 1; i++) {
        ComputePassDescription desc;
        const int mip = i + 1;
        desc.name = "Bloom downsample mip " + std::to_string(mip);
        desc.shaderDescription.srcPathRelative = "bloomDownsample.comp";
        m_bloomDownsamplePasses.push_back(gRenderBackend.createComputePass(desc));
    }
    // bloom upsample
    for (int i = 0; i < bloomMipCount - 1; i++) {
        ComputePassDescription desc;
        const int mip = bloomMipCount - 2 - i;
        desc.name = "Bloom Upsample mip " + std::to_string(mip);
        desc.shaderDescription.srcPathRelative = "bloomUpsample.comp";

        const bool isLowestMip = i == 0;
        desc.shaderDescription.specialisationConstants = {
            SpecialisationConstant{
                0,                                                          //location
                dataToCharArray((void*)&isLowestMip, sizeof(isLowestMip))}  //value
        };

        m_bloomUpsamplePasses.push_back(gRenderBackend.createComputePass(desc));
    }
    // apply bloom pass
    {
        ComputePassDescription desc;
        desc.name = "Apply bloom";
        desc.shaderDescription.srcPathRelative = "applyBloom.comp";
        m_applyBloomPass = gRenderBackend.createComputePass(desc);
    }
    // bloom textures
    {
        ImageDescription desc;
        desc.width = textureWidth;
        desc.height = textureHeight;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::R11G11B10_uFloat;
        desc.usageFlags = ImageUsageFlags::Sampled | ImageUsageFlags::Storage;
        desc.mipCount = MipCount::Manual;
        desc.manualMipCount = bloomMipCount;
        desc.autoCreateMips = false;
        m_bloomUpscaleTexture = gRenderBackend.createImage(desc, nullptr, 0);
        m_bloomDownscaleTexture = gRenderBackend.createImage(desc, nullptr, 0);
    }
}

void Bloom::resizeTextures(const int width, const int height) {
    gRenderBackend.resizeImages({
        m_bloomDownscaleTexture,
        m_bloomUpscaleTexture
    }, width, height);
}

void Bloom::computeBloom(const ImageHandle targetImage, const BloomSettings& settings) const {

    const ImageDescription bloomImageDescription = gRenderBackend.getImageDescription(m_bloomUpscaleTexture);
    const int width = bloomImageDescription.width;
    const int height = bloomImageDescription.height;

    // downscale
    for (int i = 0; i < m_bloomDownsamplePasses.size(); i++) {
        ComputePassExecution exe;
        exe.genericInfo.handle = m_bloomDownsamplePasses[i];

        const int sourceMip = i;
        const int targetMip = i + 1;

        const ImageHandle srcTexture = i == 0 ? targetImage : m_bloomDownscaleTexture;

        exe.genericInfo.resources.storageImages = {
            ImageResource(m_bloomDownscaleTexture, targetMip, 0)
        };
        exe.genericInfo.resources.sampledImages = {
            ImageResource(srcTexture, sourceMip, 1)
        };

        const glm::ivec2 baseResolution = glm::ivec2(width, height);
        const glm::ivec2 targetResolution = resolutionFromMip(glm::ivec3(baseResolution, 1), targetMip);

        const int groupSize = 8;
        exe.dispatchCount[0] = (uint32_t)glm::ceil(targetResolution.x / float(groupSize));
        exe.dispatchCount[1] = (uint32_t)glm::ceil(targetResolution.y / float(groupSize));
        exe.dispatchCount[2] = 1;

        gRenderBackend.setComputePassExecution(exe);
    }
    // upscale
    for (int i = 0; i < m_bloomUpsamplePasses.size(); i++) {
        ComputePassExecution exe;
        exe.genericInfo.handle = m_bloomUpsamplePasses[i];

        const int targetMip = bloomMipCount - 2 - i;
        const int sourceMip = targetMip + 1;

        exe.genericInfo.resources.storageImages = {
            ImageResource(m_bloomUpscaleTexture, targetMip, 0)
        };
        exe.genericInfo.resources.sampledImages = {
            ImageResource(m_bloomUpscaleTexture, sourceMip, 1),
            ImageResource(m_bloomDownscaleTexture, sourceMip, 2)
        };

        const glm::ivec2 baseResolution = glm::ivec2(width, height);
        const glm::ivec2 targetResolution = resolutionFromMip(glm::ivec3(baseResolution, 1), targetMip);

        const int groupSize = 8;
        exe.dispatchCount[0] = (uint32_t)glm::ceil(targetResolution.x / float(groupSize));
        exe.dispatchCount[1] = (uint32_t)glm::ceil(targetResolution.y / float(groupSize));
        exe.dispatchCount[2] = 1;

        exe.pushConstants = dataToCharArray((void*)&settings.radius, sizeof(settings.radius));

        gRenderBackend.setComputePassExecution(exe);
    }
    // apply bloom
    {
        ComputePassExecution exe;
        exe.genericInfo.handle = m_applyBloomPass;

        exe.genericInfo.resources.storageImages = {
            ImageResource(targetImage, 0, 0)
        };
        exe.genericInfo.resources.sampledImages = {
            ImageResource(m_bloomUpscaleTexture, 0, 1)
        };

        const int groupSize = 8;
        exe.dispatchCount[0] = (uint32_t)glm::ceil(width / float(groupSize));
        exe.dispatchCount[1] = (uint32_t)glm::ceil(height / float(groupSize));
        exe.dispatchCount[2] = 1;

        exe.pushConstants = dataToCharArray((void*)&settings.strength, sizeof(settings.strength));

        gRenderBackend.setComputePassExecution(exe);
    }
}