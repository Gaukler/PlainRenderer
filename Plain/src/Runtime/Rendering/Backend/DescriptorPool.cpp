#include "pch.h"
#include "DescriptorPool.h"
#include "VulkanContext.h"

const DescriptorPoolAllocationSizes gInitialAllocationSizes = {
    128, // set count
    128, // imageSampled
    128, // imageStorage
    128, // uniformBuffer
    128, // storageBuffer
    128  // sampler
};

DescriptorPool createDescriptorPool() {

    const uint32_t typeCount = 5;

    VkDescriptorPoolCreateInfo poolInfo;
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = 0;
    poolInfo.maxSets = gInitialAllocationSizes.setCount;
    poolInfo.poolSizeCount = typeCount;

    VkDescriptorPoolSize poolSize[typeCount];
    poolSize[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSize[0].descriptorCount = gInitialAllocationSizes.imageSampled;

    poolSize[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize[1].descriptorCount = gInitialAllocationSizes.imageStorage;

    poolSize[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize[2].descriptorCount = gInitialAllocationSizes.uniformBuffer;

    poolSize[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize[3].descriptorCount = gInitialAllocationSizes.storageBuffer;

    poolSize[4].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSize[4].descriptorCount = gInitialAllocationSizes.sampler;

    poolInfo.pPoolSizes = poolSize;

    DescriptorPool pool;
    pool.freeAllocations = gInitialAllocationSizes;
    auto res = vkCreateDescriptorPool(vkContext.device, &poolInfo, nullptr, &pool.vkPool);
    checkVulkanResult(res);

    return pool;
}

bool hasDescriptorPoolEnoughFreeAllocations(const DescriptorPool& pool, const DescriptorPoolAllocationSizes& requiredSizes) {
    return
        pool.freeAllocations.setCount >= requiredSizes.setCount &&
        pool.freeAllocations.imageSampled >= requiredSizes.imageSampled &&
        pool.freeAllocations.imageStorage >= requiredSizes.imageStorage &&
        pool.freeAllocations.storageBuffer >= requiredSizes.storageBuffer &&
        pool.freeAllocations.uniformBuffer >= requiredSizes.uniformBuffer &&
        pool.freeAllocations.sampler >= requiredSizes.sampler;
};

DescriptorPoolAllocationSizes subtractDescriptorPoolSizes(
    const DescriptorPoolAllocationSizes& first, 
    const DescriptorPoolAllocationSizes& second) {

    DescriptorPoolAllocationSizes result;
    result.setCount = first.setCount - second.setCount;
    result.imageSampled = first.imageSampled - second.imageSampled;
    result.imageStorage = first.imageStorage - second.imageStorage;
    result.storageBuffer = first.storageBuffer - second.storageBuffer;
    result.uniformBuffer = first.uniformBuffer - second.uniformBuffer;
    result.sampler = first.sampler - second.sampler;
    return result;
};