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

    // downscales and blurs screen sized target image in separate texture, then additively blends on top of targetImage
    void computeBloom(const ImageHandle targetImage, const BloomSettings& settings) const;

private:

    std::vector<RenderPassHandle> m_bloomDownsamplePasses;
    std::vector<RenderPassHandle> m_bloomUpsamplePasses;

    RenderPassHandle m_applyBloomPass;
};