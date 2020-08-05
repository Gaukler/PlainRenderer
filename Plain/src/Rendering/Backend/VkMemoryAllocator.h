#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "VulkanAllocation.h"

//move into allocator later
//curently needed from backend because it handles the staging buffer manually
uint32_t findMemoryIndex(const VkMemoryPropertyFlags flags);

//kept internally, forms a linked list
struct VulkanAllocationInternal {
    bool                isFree = true;
    VkDeviceSize        offset;
    VkDeviceSize        size;
    VulkanAllocationInternal* previous = nullptr;
    VulkanAllocationInternal* next = nullptr;
};

/*
keeps a linked list of allocations (AllocationInternal)
when a block of memory is allocated the free block is split into the allocated and the remaining free memory
when a block is freed the neighbours are checked and merged if possible
*/
class VkMemoryPool {
public:
    //must be called before allocation
    bool create();
    void destroy();

    bool allocate(const VkDeviceSize size, const VkDeviceSize alignment, VulkanAllocation* outAllocation);
    void free(const VulkanAllocation& allocation);

    uint32_t getUsedMemorySize();
    uint32_t getAllocatedMemorySize();
private:
    const VkDeviceSize m_initialSize = 268435456; //256 mb
    VkDeviceSize m_freeMemorySize = m_initialSize;

    VkDeviceMemory      m_vulkanMemory;
    VulkanAllocationInternal* m_head; //linked allocation list head
};

/*
has a list of memory pools, if no pool can allocate the memory a new pool is created
for now device local memory only, staging buffer is handled by backend manually
*/
class VkMemoryAllocator {
public:
    bool create();
    void destroy();

    bool allocate(const VkDeviceSize size, const VkDeviceSize alignment, VulkanAllocation* outAllocation);
    void free(const VulkanAllocation& allocation);

    void getMemoryStats(uint32_t* outAllocatedSize, uint32_t* outUsedSize);
private:
    std::vector<VkMemoryPool> m_memoryPools;
};