#include "pch.h"
#include "UniformBufferManager.h"
#include "Buffer.h"

UniformBufferManager& UniformBufferManager::getRef() {
    static UniformBufferManager instance;
    return instance;
}

void UniformBufferManager::shutdown() {
    for (const auto& buffer : m_uniformBuffers) {
        destroyVulkanBuffer(buffer);
    }
    m_uniformBuffers.clear();
}

UniformBufferHandle UniformBufferManager::addUniformBuffer(const Buffer& buffer) {
    UniformBufferHandle handle = { (uint32_t)m_uniformBuffers.size() };
    m_uniformBuffers.push_back(buffer);
    return handle;
}

Buffer& UniformBufferManager::getUniformBufferRef(UniformBufferHandle handle) {
    return m_uniformBuffers[handle.index];
}