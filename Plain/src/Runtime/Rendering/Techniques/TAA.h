#pragma once
#include "pch.h"
#include "Runtime/Rendering/Backend/RenderBackend.h"

enum class HistorySamplingTech : int { Bilinear = 0, Bicubic16Tap = 1, Bicubic9Tap = 2, Bicubic5Tap = 3, Bicubic1Tap = 4 };

struct TAASettings {
    bool enabled = true;
    bool useSeparateSupersampling = false;
    bool useClipping = true;
    bool useMotionVectorDilation = true;
    HistorySamplingTech historySamplingTech = HistorySamplingTech::Bicubic1Tap;
    bool supersampleUseTonemapping = true;
    bool filterUseTonemapping = true;
    bool useMipBias = true;
};

//simple wrapper to keep all images and framebuffers used in a frame in one place
//simplifies keeping resources of multiples frames around for temporal techniques
struct FrameRenderTargets {
    ImageHandle colorBuffer;
    ImageHandle motionBuffer;
    ImageHandle depthBuffer;
    FramebufferHandle colorFramebuffer;
    FramebufferHandle prepassFramebuffer;
};

class TAA {
public:
    void init(const int imageWidth, const int imageHeight, const TAASettings& settings);
    void resizeImages(const int width, const int height);

    void updateSettings(const TAASettings& settings);

    RenderPassHandle computeTemporalSuperSampling(const FrameRenderTargets& currentFrame, const FrameRenderTargets& lastFrame,
        const ImageHandle target, const RenderPassHandle parent) const;

    RenderPassHandle computeTemporalFilter(const ImageHandle colorSrc, const FrameRenderTargets& currentFrame, const ImageHandle target, 
        const RenderPassHandle parent) const;

    // returns jitter in pixels, must be multiplied with texel size before applying to projection matrix
    glm::vec2 computeProjectionMatrixJitter() const;

    // jitter must be in screen coordinates, jitter in pixels must first be scaled by pixel size
    glm::mat4 applyProjectionMatrixJitter(const glm::mat4& projectionMatrix, const glm::vec2& offset) const;

    // jitter must be in pixels, so before scaling with pixel size
    void updateTaaResolveWeights(const glm::vec2 cameraJitterInPixels);

private:

    ShaderDescription createTemporalFilterShaderDescription(const TAASettings& settings) const;
    ShaderDescription createTemporalSupersamplingShaderDescription(const TAASettings& settings) const;

    RenderPassHandle m_temporalSupersamplingPass;
    RenderPassHandle m_temporalFilterPass;
    RenderPassHandle m_colorToLuminancePass;

    ImageHandle m_historyBuffers[2];
    ImageHandle m_sceneLuminance[2];

    UniformBufferHandle m_taaResolveWeightBuffer;
};