#pragma once
#include "pch.h"
#include "Resources.h"
#include "../ResourceDescriptions.h"

class StorageBufferManager {
public:
    static StorageBufferManager& getRef();
    StorageBufferManager(const StorageBufferManager&) = delete;

    void shutdown();

    StorageBufferHandle addStorageBuffer(const Buffer& buffer);
    Buffer& getStorageBufferRef(StorageBufferHandle handle);
private:
    StorageBufferManager() {};
    std::vector<Buffer> m_storageBuffers;
};