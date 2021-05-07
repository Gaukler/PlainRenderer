#pragma once
#include "pch.h"
#include "Resources.h"
#include "../ResourceDescriptions.h"

class UniformBufferManager {
public:
    static UniformBufferManager& getRef();
    UniformBufferManager(const UniformBufferManager&) = delete;

    void shutdown();

    UniformBufferHandle addUniformBuffer(const Buffer& buffer);
    Buffer& getUniformBufferRef(UniformBufferHandle handle);
private:
    UniformBufferManager() {};
    std::vector<Buffer> m_uniformBuffers;
};