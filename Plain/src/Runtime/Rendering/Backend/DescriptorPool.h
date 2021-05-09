#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

struct DescriptorPoolAllocationSizes {
    uint32_t setCount = 0;
    uint32_t imageSampled = 0;
    uint32_t imageStorage = 0;
    uint32_t uniformBuffer = 0;
    uint32_t storageBuffer = 0;
    uint32_t sampler = 0;
};

struct DescriptorPool {
    VkDescriptorPool vkPool = VK_NULL_HANDLE;
    DescriptorPoolAllocationSizes freeAllocations;
};

DescriptorPool createDescriptorPool();
bool hasDescriptorPoolEnoughFreeAllocations(const DescriptorPool& pool, const DescriptorPoolAllocationSizes& requiredSizes);
DescriptorPoolAllocationSizes subtractDescriptorPoolSizes(
    const DescriptorPoolAllocationSizes& first, 
    const DescriptorPoolAllocationSizes& second);