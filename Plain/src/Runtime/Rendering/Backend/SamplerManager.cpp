#include "SamplerManager.h"
#include "VulkanContext.h"

SamplerManager& SamplerManager::getRef() {
    static SamplerManager instance;
    return instance;
}

SamplerHandle SamplerManager::addSampler(const VkSampler sampler) {
    SamplerHandle handle = { (uint32_t)m_samplers.size() };
    m_samplers.push_back(sampler);
    return handle;
}

VkSampler SamplerManager::getSampler(const SamplerHandle handle) {
    return m_samplers[handle.index];
}

void SamplerManager::shutdown() {
    for (const auto& sampler : m_samplers) {
        vkDestroySampler(vkContext.device, sampler, nullptr);
    }
}