#include "pch.h"
#include "TAA.h"
#include "Common/Utilities/GeneralUtils.h"
#include "Runtime/FrameIndex.h"
#include <Utilities/MathUtils.h>

void TAA::init(const int imageWidth, const int imageHeight, const TAASettings& settings) {
    // temporal filter pass
    {
        ComputePassDescription desc;
        desc.name = "Temporal filtering";
        desc.shaderDescription = createTemporalFilterShaderDescription(settings);
        m_temporalFilterPass = gRenderBackend.createComputePass(desc);
    }
    // temporal supersampling pass
    {
        ComputePassDescription desc;
        desc.name = "Temporal supersampling";
        desc.shaderDescription = createTemporalSupersamplingShaderDescription(settings);
        m_temporalSupersamplingPass = gRenderBackend.createComputePass(desc);
    }
    // history buffers
    {
        ImageDescription desc;
        desc.width = imageWidth;
        desc.height = imageHeight;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::R11G11B10_uFloat;
        desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_historyBuffers[0] = gRenderBackend.createImage(desc, nullptr, 0);
        m_historyBuffers[1] = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // scene and history luminance
    {
        ImageDescription desc;
        desc.width = imageWidth;
        desc.height = imageHeight;
        desc.depth = 1;
        desc.type = ImageType::Type2D;
        desc.format = ImageFormat::R8;
        desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
        desc.mipCount = MipCount::One;
        desc.manualMipCount = 1;
        desc.autoCreateMips = false;

        m_sceneLuminance[0] = gRenderBackend.createImage(desc, nullptr, 0);
        m_sceneLuminance[1] = gRenderBackend.createImage(desc, nullptr, 0);
    }
    // taa resolve weight buffer
    {
        UniformBufferDescription desc;
        desc.size = sizeof(float) * 9;
        m_taaResolveWeightBuffer = gRenderBackend.createUniformBuffer(desc);
    }
    //color to luminance pass
    {
        ComputePassDescription desc;
        desc.name = "Color to Luminance";
        desc.shaderDescription.srcPathRelative = "colorToLuminance.comp";
        m_colorToLuminancePass = gRenderBackend.createComputePass(desc);
    }
}

void TAA::resizeImages(const int width, const int height) {
    gRenderBackend.resizeImages({
        m_historyBuffers[0],
        m_historyBuffers[1],
        m_sceneLuminance[0],
        m_sceneLuminance[1]
        }, 
        width, 
        height);
}

void TAA::updateSettings(const TAASettings& settings) {
    gRenderBackend.updateComputePassShaderDescription(m_temporalFilterPass, createTemporalFilterShaderDescription(settings));
    gRenderBackend.updateComputePassShaderDescription(m_temporalSupersamplingPass, createTemporalSupersamplingShaderDescription(settings));
}

void TAA::computeTemporalSuperSampling(const FrameRenderTargets& currentFrame, const FrameRenderTargets& lastFrame,
    const ImageHandle target) const {
        
    const ImageDescription targetDescription = gRenderBackend.getImageDescription(target);

    const size_t frameIndexMod2 = FrameIndex::getFrameIndexMod2();
    const ImageHandle currentLuminance = m_sceneLuminance[frameIndexMod2];
    const ImageHandle historyLuminance = m_sceneLuminance[(frameIndexMod2 + 1) % 2];

    // scene luminance
    {
        const ImageResource srcResource(currentFrame.colorBuffer, 0, 0);
        const ImageResource dstResource(currentLuminance, 0, 1);

        ComputePassExecution exe;
        exe.genericInfo.handle = m_colorToLuminancePass;
        exe.genericInfo.resources.storageImages = { dstResource };
        exe.genericInfo.resources.sampledImages = { srcResource };
        exe.dispatchCount[0] = (uint32_t)std::ceil(targetDescription.width  / 8.f);
        exe.dispatchCount[1] = (uint32_t)std::ceil(targetDescription.height / 8.f);
        exe.dispatchCount[2] = 1;

        gRenderBackend.setComputePassExecution(exe);
    }
    // temporal supersampling
    {
        const ImageResource currentFrameResource(currentFrame.colorBuffer, 0, 1);
        const ImageResource lastFrameResource(lastFrame.colorBuffer, 0, 2);
        const ImageResource targetResource(target, 0, 3);
        const ImageResource velocityBufferResource(currentFrame.motionBuffer, 0, 4);
        const ImageResource currentDepthResource(currentFrame.depthBuffer, 0, 5);
        const ImageResource lastDepthResource(lastFrame.depthBuffer, 0, 6);
        const ImageResource currentLuminanceResource(currentLuminance, 0, 7);
        const ImageResource lastLuminanceResource(historyLuminance, 0, 8);

        ComputePassExecution temporalSupersamplingExecution;
        temporalSupersamplingExecution.genericInfo.handle = m_temporalSupersamplingPass;
        temporalSupersamplingExecution.genericInfo.resources.storageImages = { targetResource };
        temporalSupersamplingExecution.genericInfo.resources.sampledImages = {
            currentFrameResource,
            lastFrameResource,
            velocityBufferResource,
            currentDepthResource,
            lastDepthResource,
            currentLuminanceResource,
            lastLuminanceResource };
        temporalSupersamplingExecution.dispatchCount[0] = (uint32_t)std::ceil(targetDescription.width  / 8.f);
        temporalSupersamplingExecution.dispatchCount[1] = (uint32_t)std::ceil(targetDescription.height / 8.f);
        temporalSupersamplingExecution.dispatchCount[2] = 1;

        gRenderBackend.setComputePassExecution(temporalSupersamplingExecution);
    }
}

void TAA::computeTemporalFilter(const ImageHandle colorSrc, const FrameRenderTargets& currentFrame, 
    const ImageHandle target) const {

    const size_t frameIndexMod2 = FrameIndex::getFrameIndexMod2();
    const ImageHandle historySrc = m_historyBuffers[frameIndexMod2];
    const ImageHandle historyDst = m_historyBuffers[(frameIndexMod2 + 1) % 2];

    const ImageResource inputImageResource(colorSrc, 0, 0);
    const ImageResource outputImageResource(target, 0, 1);
    const ImageResource historyDstResource(historyDst, 0, 2);
    const ImageResource historySrcResource(historySrc, 0, 3);
    const ImageResource motionBufferResource(currentFrame.motionBuffer, 0, 4);
    const ImageResource depthBufferResource(currentFrame.depthBuffer, 0, 5);
    const UniformBufferResource resolveWeightsResource(m_taaResolveWeightBuffer, 6);

    const ImageDescription targetDescription = gRenderBackend.getImageDescription(target);

    ComputePassExecution temporalFilterExecution;
    temporalFilterExecution.genericInfo.handle = m_temporalFilterPass;
    temporalFilterExecution.genericInfo.resources.storageImages = { outputImageResource, historyDstResource };
    temporalFilterExecution.genericInfo.resources.sampledImages = { inputImageResource, historySrcResource, motionBufferResource, depthBufferResource };
    temporalFilterExecution.genericInfo.resources.uniformBuffers = { resolveWeightsResource };
    temporalFilterExecution.dispatchCount[0] = (uint32_t)std::ceil(targetDescription.width  / 8.f);
    temporalFilterExecution.dispatchCount[1] = (uint32_t)std::ceil(targetDescription.height / 8.f);
    temporalFilterExecution.dispatchCount[2] = 1;

    gRenderBackend.setComputePassExecution(temporalFilterExecution);
}

glm::vec2 TAA::computeProjectionMatrixJitter() const {
    return 2.f * hammersley2D(FrameIndex::getFrameIndexMod8()) - 1.f;
}

glm::mat4 TAA::applyProjectionMatrixJitter(const glm::mat4& projectionMatrix, const glm::vec2& offset) const {

    glm::mat4 jitteredProjection = projectionMatrix;
    jitteredProjection[2][0] = offset.x;
    jitteredProjection[2][1] = offset.y;

    return jitteredProjection;
}

void TAA::updateTaaResolveWeights(const glm::vec2 cameraJitterInPixels) {
    std::array<float, 9> weights = {};
    int index = 0;
    float totalWeight = 0.f;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            const float d = glm::length(cameraJitterInPixels - glm::vec2(x, y));
            // gaussian fit to blackman-Harris 3.3
            // reference: "High Quality Temporal Supersampling", page 23
            const float w = glm::exp(-2.29f * d * d);
            weights[index] = w;
            totalWeight += w;
            index++;
        }
    }

    for (float& w : weights) {
        w /= totalWeight;
    }

    gRenderBackend.setUniformBufferData(m_taaResolveWeightBuffer, &weights[0], sizeof(float) * 9);
}

ShaderDescription TAA::createTemporalFilterShaderDescription(const TAASettings& settings) const{
    ShaderDescription desc;
    desc.srcPathRelative = "temporalFilter.comp";

    // specialisation constants
    {
        // use clipping
        desc.specialisationConstants.push_back({
            0,                                                                      // location
            dataToCharArray(&settings.useClipping, sizeof(settings.useClipping))    // value
            });
        // use use motion vector dilation
        desc.specialisationConstants.push_back({
            1,                                                                                              // location
            dataToCharArray(&settings.useMotionVectorDilation, sizeof(settings.useMotionVectorDilation))    // value
            });
        // history sampling tech
        desc.specialisationConstants.push_back({
            2,                                                                                      // location
            dataToCharArray(&settings.historySamplingTech, sizeof(settings.historySamplingTech))    // value
            });
        // using tonemapping
        desc.specialisationConstants.push_back({
            3,                                                                                      // location
            dataToCharArray(&settings.filterUseTonemapping, sizeof(settings.filterUseTonemapping))  // value
            });
    }

    return desc;
}

ShaderDescription TAA::createTemporalSupersamplingShaderDescription(const TAASettings& settings) const{
    ShaderDescription desc;
    desc.srcPathRelative = "temporalSupersampling.comp";

    // specialisation constant: using tonemapping
    desc.specialisationConstants.push_back({
        0,                                                                                                      // location
        dataToCharArray((void*)&settings.supersampleUseTonemapping, sizeof(settings.supersampleUseTonemapping)) // value
    });

    return desc;
}