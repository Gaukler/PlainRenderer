#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "VulkanAllocation.h"

//kept internally, forms a linked list
struct VulkanAllocationInternal {
    bool                isFree = true;
    VkDeviceSize        offset;
    VkDeviceSize        size;
    VulkanAllocationInternal* previous = nullptr;
    VulkanAllocationInternal* next = nullptr;
};


//keeps a linked list of allocations (AllocationInternal)
//when a block of memory is allocated the free block is split into the allocated and the remaining free memory
//when a block is freed the neighbours are checked and merged if possible
class VkMemoryPool {
public:
    //must be called before allocation
    bool create(const uint32_t memoryIndex);
    void destroy();

    bool allocate(const VkDeviceSize size, const VkDeviceSize alignment, VulkanAllocation* outAllocation);
    void free(const VulkanAllocation& allocation);

    uint32_t getUsedMemorySize() const;
    uint32_t getAllocatedMemorySize() const;
private:
    const VkDeviceSize m_initialSize = 268435456; //256 mb
    VkDeviceSize m_freeMemorySize = m_initialSize;

    VkDeviceMemory      m_vulkanMemory;
    VulkanAllocationInternal* m_head; //linked allocation list head
};

//has a list of memory pools, if no pool can allocate the memory a new pool is created
class VkMemoryAllocator {
public:
    void create();
    void destroy();

    bool allocate(const VkMemoryRequirements& requirements, const VkMemoryPropertyFlags flags, VulkanAllocation* outAllocation);
    void free(const VulkanAllocation& allocation);

    void getMemoryStats(uint32_t* outAllocatedSize, uint32_t* outUsedSize);
private:
    uint32_t findMemoryIndex(const VkMemoryPropertyFlags flags, const uint32_t memoryTypeBitsRequirement);

    std::vector<std::vector<VkMemoryPool>> m_memoryPoolsPerMemoryIndex;
};