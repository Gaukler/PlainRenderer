#include "pch.h"
#include "VkMemoryAllocator.h"
#include "VulkanContext.h"

VulkanContext vkContext;

bool VkMemoryPool::create(const uint32_t memoryIndex) {
    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.allocationSize = m_initialSize;

    allocateInfo.memoryTypeIndex = memoryIndex;

    const auto res = vkAllocateMemory(vkContext.device, &allocateInfo, nullptr, &m_vulkanMemory);
    bool success = res == VK_SUCCESS;

    m_head = new VulkanAllocationInternal();
    m_head->offset = 0;
    m_head->size = m_freeMemorySize;

    return success;
}

void VkMemoryPool::destroy() {
    vkFreeMemory(vkContext.device, m_vulkanMemory, nullptr);
}

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

VkDeviceSize VkMemoryPool::getUsedMemorySize() const{
    return m_initialSize - m_freeMemorySize;
}

VkDeviceSize VkMemoryPool::getAllocatedMemorySize() const {
    return m_initialSize;
}

void VkMemoryAllocator::create() {
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(vkContext.physicalDevice, &memoryProperties);
    m_memoryPoolsPerMemoryIndex.resize(memoryProperties.memoryTypeCount);
}

void VkMemoryAllocator::destroy() {
    for (auto& poolList : m_memoryPoolsPerMemoryIndex) {
        for (auto& pool : poolList) {
            pool.destroy();
        }
    }
}

bool VkMemoryAllocator::allocate(const VkMemoryRequirements& requirements, const VkMemoryPropertyFlags flags, VulkanAllocation* outAllocation) {

    //find memory index
    outAllocation->memoryIndex = findMemoryIndex(flags, requirements.memoryTypeBits);

    //try to allocate with the existing pools
    auto& poolList = m_memoryPoolsPerMemoryIndex[outAllocation->memoryIndex];
    uint32_t poolIndex = 0;
    for (auto& pool : poolList) {
        if (pool.allocate(requirements.size, requirements.alignment, outAllocation)) {
            outAllocation->poolIndex = poolIndex;
            return true;
        }
        poolIndex++;
    }
    //try allocation with a new pool
    VkMemoryPool newPool;
    if (newPool.create(outAllocation->memoryIndex)) {
        poolList.push_back(newPool);
        bool success = poolList.back().allocate(requirements.size, requirements.alignment, outAllocation);
        outAllocation->poolIndex = (uint32_t)poolList.size() - 1;
        return success;
    }
    else {
        return false;
    }
}

void VkMemoryAllocator::free(const VulkanAllocation& allocation) {
    auto& poolList = m_memoryPoolsPerMemoryIndex[allocation.memoryIndex];
    poolList[allocation.poolIndex].free(allocation);
}

void VkMemoryAllocator::getMemoryStats(VkDeviceSize* outAllocatedSize, VkDeviceSize* outUsedSize) {
    assert(outAllocatedSize != nullptr);
    assert(outUsedSize != nullptr);
    *outAllocatedSize = 0;
    *outUsedSize = 0;
    for (const auto& poolList : m_memoryPoolsPerMemoryIndex) {
        for (const auto& pool : poolList) {
            *outAllocatedSize += pool.getAllocatedMemorySize();
            *outUsedSize += pool.getUsedMemorySize();
        }
    }
}

uint32_t VkMemoryAllocator::findMemoryIndex(const VkMemoryPropertyFlags flags, const uint32_t memoryTypeBitsRequirement) {

    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(vkContext.physicalDevice, &memoryProperties);

    const auto findIndex = [&memoryProperties, memoryTypeBitsRequirement](const VkMemoryPropertyFlags flags, uint32_t* outIndex) {
        for (uint32_t memoryIndex = 0; memoryIndex < memoryProperties.memoryTypeCount; memoryIndex++) {
            const bool hasRequiredProperties = (flags == 0) || (memoryProperties.memoryTypes[memoryIndex].propertyFlags & flags);

            const uint32_t memoryTypeBits = (1 << memoryIndex);
            const bool isRequiredMemoryType = memoryTypeBitsRequirement & memoryTypeBits;
            if (hasRequiredProperties && isRequiredMemoryType) {
                *outIndex = memoryIndex;
                return true;
            }
        }
        return false;
    };

    //first try
    uint32_t memoryIndex = 0;
    if (findIndex(flags, &memoryIndex)) {
        return memoryIndex;
    }

    //if device local is a property try finding memory without it
    //this way it works on machines without dedicated memory, like integrated GPUs
    if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
        const VkMemoryPropertyFlags flagWithoutDeviceLocal = flags & !VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        if (findIndex(flagWithoutDeviceLocal, &memoryIndex)) {
            return memoryIndex;
        }
    }

    throw std::runtime_error("failed to find adequate memory type for allocation");
}