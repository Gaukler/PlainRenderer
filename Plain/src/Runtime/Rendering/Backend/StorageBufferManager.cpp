#include "pch.h"
#include "StorageBufferManager.h"
#include "Buffer.h"

StorageBufferManager& StorageBufferManager::getRef() {
    static StorageBufferManager instance;
    return instance;
}

void StorageBufferManager::shutdown() {
    for (const auto& buffer : m_storageBuffers) {
        destroyVulkanBuffer(buffer);
    }
    m_storageBuffers.clear();
}

StorageBufferHandle StorageBufferManager::addStorageBuffer(const Buffer& buffer) {
    StorageBufferHandle handle = { (uint32_t)m_storageBuffers.size() };
    m_storageBuffers.push_back(buffer);
    return handle;
}

Buffer& StorageBufferManager::getStorageBufferRef(StorageBufferHandle handle) {
    return m_storageBuffers[handle.index];
}