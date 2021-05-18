#include "pch.h"
#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "Common/Utilities/GeneralUtils.h"

VkBuffer createVulkanBuffer(const size_t size, const VkBufferUsageFlags usageFlags, 
    const std::vector<uint32_t> & uniqueQueueFamilies) {

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType                    = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext                    = nullptr;
    bufferInfo.flags                    = 0;
    bufferInfo.size                     = size;
    bufferInfo.usage                    = usageFlags;
    bufferInfo.queueFamilyIndexCount    = (uint32_t)uniqueQueueFamilies.size();
    bufferInfo.pQueueFamilyIndices      = uniqueQueueFamilies.data();

    const bool usedByMultipleQueues = uniqueQueueFamilies.size() > 1;
    if (usedByMultipleQueues) {
        bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
    }
    else {
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkBuffer buffer;
    const auto result = vkCreateBuffer(vkContext.device, &bufferInfo, nullptr, &buffer);
    checkVulkanResult(result);
    return buffer;
}

VulkanAllocation allocateAndBindBufferMemory(const VkBuffer buffer, const VkMemoryAllocateFlags memoryFlags, 
    VkMemoryAllocator &allocator) {

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(vkContext.device, buffer, &memoryRequirements);

    VulkanAllocation memoryAllocation;
    if (!allocator.allocate(memoryRequirements, memoryFlags, &memoryAllocation)) {
        throw("Could not allocate buffer memory");
    }

    const auto result = vkBindBufferMemory(vkContext.device, buffer, memoryAllocation.vkMemory, memoryAllocation.offset);
    checkVulkanResult(result);

    return memoryAllocation;
}

std::vector<uint32_t> makeUniqueQueueFamilyList(const std::vector<uint32_t>& queueFamilies) {
    std::vector<uint32_t> uniqueFamilyList;
    for (const uint32_t& familyIndex : queueFamilies) {
        const bool indexAlreadyListed = vectorContains(uniqueFamilyList, familyIndex);
        if (!indexAlreadyListed) {
            uniqueFamilyList.push_back(familyIndex);
        }
    }
    return uniqueFamilyList;
}