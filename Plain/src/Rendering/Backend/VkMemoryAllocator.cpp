#include "pch.h"
#include "VkMemoryAllocator.h"
#include "VulkanContext.h"

VulkanContext vkContext;

/*
=========
findMemoryIndex
=========
*/
uint32_t findMemoryIndex(const VkMemoryPropertyFlags flags) {

    uint32_t memoryIndex = 0;
    bool foundMemory = false;

    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(vkContext.physicalDevice, &memoryProperties);

    //search for appropriate memory type
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((flags == 0) || (memoryProperties.memoryTypes[i].propertyFlags & flags)) {
            memoryIndex = i;
            foundMemory = true;
            break;
        }
    }

    if (!foundMemory) {
        throw std::runtime_error("failed to find adequate memory type for allocation");
    }

    return memoryIndex;
}

/*
==================

VkMemoryPool

==================
*/

/*
=========
create
=========
*/
bool VkMemoryPool::create() {
    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.allocationSize = m_initialSize;

    allocateInfo.memoryTypeIndex = findMemoryIndex(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    const auto res = vkAllocateMemory(vkContext.device, &allocateInfo, nullptr, &m_vulkanMemory);
    bool success = res == VK_SUCCESS;

    m_head = new VulkanAllocationInternal();
    m_head->offset = 0;
    m_head->size = m_freeMemorySize;

    return success;
}

/*
=========
destroy
=========
*/
void VkMemoryPool::destroy() {
    vkFreeMemory(vkContext.device, m_vulkanMemory, nullptr);
}

/*
=========
allocate
=========
*/
bool VkMemoryPool::allocate(const VkDeviceSize size, const VkDeviceSize alignment, VulkanAllocation* outAllocation) {
    assert(outAllocation != nullptr);
    if (size > m_freeMemorySize) {
        return false;
    }

    //seach for big enough free memory block
    VulkanAllocationInternal* currentAlloc = m_head;
    while (currentAlloc != nullptr) {
        if (currentAlloc->isFree) {
            //pad to alignment
            VkDeviceSize padding = (alignment - (currentAlloc->offset % alignment)) % alignment;
            VkDeviceSize paddedSize = size + padding;
            if (paddedSize <= currentAlloc->size) {
                //large enough allocation found
                m_freeMemorySize -= paddedSize;

                //fill out allocation
                outAllocation->vkMemory = m_vulkanMemory;
                outAllocation->offset = currentAlloc->offset + padding;
                outAllocation->padding = padding;

                //split current block if there is remaining memory
                VkDeviceSize remainingSize = currentAlloc->size - paddedSize;
                if (remainingSize > 0) {
                    //new occupied allocation
                    VulkanAllocationInternal* occupiedAllocation = new VulkanAllocationInternal();
                    occupiedAllocation->isFree = false;
                    occupiedAllocation->size = paddedSize;
                    occupiedAllocation->offset = currentAlloc->offset;

                    //remaining allocation
                    VulkanAllocationInternal* remainingAllocation = new VulkanAllocationInternal();
                    remainingAllocation->isFree = true;
                    remainingAllocation->size = remainingSize;
                    remainingAllocation->offset = occupiedAllocation->offset + occupiedAllocation->size;

                    //update pointers
                    occupiedAllocation->next = remainingAllocation;
                    occupiedAllocation->previous = currentAlloc->previous;

                    remainingAllocation->next = currentAlloc->next;
                    remainingAllocation->previous = occupiedAllocation;

                    if (currentAlloc->previous != nullptr) {
                        currentAlloc->previous->next = occupiedAllocation;
                    }
                    else {
                        m_head = occupiedAllocation;
                    }
                    if (currentAlloc->next != nullptr) {
                        currentAlloc->next->previous = remainingAllocation;
                    }
                }
                else {
                    currentAlloc->isFree = false;
                }
                return true;
            }
        }
        currentAlloc = currentAlloc->next;
    }
    return false;
}

/*
=========
free
=========
*/
void VkMemoryPool::free(const VulkanAllocation& allocation) {

    //helper, creates new combined allocation, updates linked list pointers and deletes inputs
    const auto mergeNeighbourAllocations = [this](VulkanAllocationInternal* first, VulkanAllocationInternal* second) {
        //only free allocations may be merged
        assert(first->isFree);
        assert(second->isFree);

        //create new merged allocation
        VulkanAllocationInternal* merged = new VulkanAllocationInternal();
        merged->isFree = true;
        merged->offset = first->offset;
        merged->size = first->size + second->size;

        //update pointers
        merged->previous = first->previous;
        merged->next = second->next;

        if (merged->previous != nullptr) {
            merged->previous->next = merged;
        }
        else {
            m_head = merged;
        }
        if (merged->next != nullptr) {
            merged->next->previous = merged;
        }
        delete first;
        delete second;
        return merged;
    };

    //find allocation
    VkDeviceSize offsetWithoutPadding = allocation.offset - allocation.padding;
    VulkanAllocationInternal* currentAlloc = m_head;
    while (currentAlloc != nullptr) {
        if (currentAlloc->offset == offsetWithoutPadding) {
            //found allocation
            currentAlloc->isFree = true;
            m_freeMemorySize += currentAlloc->size;

            //merge with neighbours if they are free
            if (currentAlloc->previous != nullptr && currentAlloc->previous->isFree) {
                currentAlloc = mergeNeighbourAllocations(currentAlloc->previous, currentAlloc);
            }
            if (currentAlloc->next != nullptr && currentAlloc->next->isFree) {
                currentAlloc = mergeNeighbourAllocations(currentAlloc, currentAlloc->next);
            }
            return;
        }
        currentAlloc = currentAlloc->next;
    }
    std::cout << "Warning: VkMemoryAllocator::free did not find the input allocation, this should not happen\n";
}

/*
=========
getUsedMemorySize
=========
*/
uint32_t VkMemoryPool::getUsedMemorySize() {
    return m_initialSize - m_freeMemorySize;
}

/*
=========
getAllocatedMemorySize
=========
*/
uint32_t VkMemoryPool::getAllocatedMemorySize() {
    return m_initialSize;
}

/*
==================

VkMemoryAllocator

==================
*/

/*
=========
create
=========
*/
bool VkMemoryAllocator::create() {
    VkMemoryPool pool;
    bool success = pool.create();
    m_memoryPools.push_back(pool);
    return success;
}

/*
=========
destroy
=========
*/
void VkMemoryAllocator::destroy() {
    for (auto& pool : m_memoryPools) {
        pool.destroy();
    }
}

/*
=========
allocate
=========
*/
bool VkMemoryAllocator::allocate(const VkDeviceSize size, const VkDeviceSize alignment, VulkanAllocation* outAllocation) {
    //try to allocate with the existing pools
    uint32_t i = 0;
    for (auto& pool : m_memoryPools) {
        if (pool.allocate(size, alignment, outAllocation)) {
            outAllocation->poolIndex = i;
            return true;
        }
        i++;
    }
    //try allocation with a new pool
    VkMemoryPool newPool;
    if (newPool.create()) {
        m_memoryPools.push_back(newPool);
        bool success = m_memoryPools.back().allocate(size, alignment, outAllocation);
        outAllocation->poolIndex = m_memoryPools.size() - 1;
        return success;
    }
    else {
        return false;
    }
}

/*
=========
free
=========
*/
void VkMemoryAllocator::free(const VulkanAllocation& allocation) {
    m_memoryPools[allocation.poolIndex].free(allocation);
}

/*
=========
getMemoryStats
=========
*/
void VkMemoryAllocator::getMemoryStats(uint32_t* outAllocatedSize, uint32_t* outUsedSize) {
    assert(outAllocatedSize != nullptr);
    assert(outUsedSize != nullptr);
    *outAllocatedSize = 0;
    *outUsedSize = 0;
    for (auto& pool : m_memoryPools) {
        *outAllocatedSize += pool.getAllocatedMemorySize();
        *outUsedSize += pool.getUsedMemorySize();
    }
}