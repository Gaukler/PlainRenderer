#pragma once
#include "pch.h"
#include "Runtime/Rendering/Backend/RenderBackend.h"

struct BloomSettings {
    bool enabled = true;
    float strength = 0.05f;
    float radius = 1.5f;
};

class Bloom {
public:
    void init(const int textureWidth, const int textureHeight);
    void resizeTextures(const int width, const int height);

    // downscales and blurs screen sized target image in separate texture, then additively blends on top of targetImage
    // returns last render pass, which must be used as parent, when accessing target image for correct ordering
    // parent pass is used as initial parent and must be last pass that used target image
    RenderPassHandle computeBloom(const RenderPassHandle parentPass, const ImageHandle targetImage, const BloomSettings& settings) const;

private:

    std::vector<RenderPassHandle> m_bloomDownsamplePasses;
    std::vector<RenderPassHandle> m_bloomUpsamplePasses;

    RenderPassHandle m_applyBloomPass;

    ImageHandle m_bloomDownscaleTexture;
    ImageHandle m_bloomUpscaleTexture;
};