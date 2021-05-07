#pragma once
#include "pch.h"
#include "Resources.h"
#include "../RenderHandles.h"

class SamplerManager {
public:
    static SamplerManager& getRef();
    SamplerManager(const SamplerManager&) = delete;

    SamplerHandle addSampler(const VkSampler sampler);
    VkSampler getSampler(const SamplerHandle handle);
    void shutdown();
private:
    SamplerManager() {};

    std::vector<VkSampler> m_samplers;
};