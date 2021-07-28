#include "pch.h"
#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "Common/Utilities/GeneralUtils.h"
#include "VulkanCommandRecording.h"
#include "VulkanCommandBuffer.h"
#include "VulkanSync.h"

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

void fillHostVisibleCoherentBuffer(const Buffer& target, const Data& data) {
    void* mappedData;
    const auto result = vkMapMemory(
        vkContext.device, 
        target.memory.vkMemory, 
        target.memory.offset, 
        data.size, 
        0, 
        (void**)&mappedData);
    checkVulkanResult(result);
    memcpy(mappedData, data.ptr, data.size);
    vkUnmapMemory(vkContext.device, target.memory.vkMemory);
}

void fillDeviceLocalBufferImmediate(const Buffer& target, const Data& data, const TransferResources& transferResources) {

    const Buffer& stagingBuffer = transferResources.stagingBuffer;

    // TODO: creation of cmd buffer and fence in loop is somewhat inefficient
    for (VkDeviceSize currentMemoryOffset = 0; currentMemoryOffset < data.size; currentMemoryOffset += stagingBuffer.size) {

        VkDeviceSize copySize = std::min(stagingBuffer.size, data.size - currentMemoryOffset);

        // copy data to staging buffer
        void* mappedData;
        auto res = vkMapMemory(vkContext.device, stagingBuffer.memory.vkMemory, 0, copySize, 0, (void**)&mappedData);
        assert(res == VK_SUCCESS);
        memcpy(mappedData, (char*)data.ptr + currentMemoryOffset, copySize);
        vkUnmapMemory(vkContext.device, stagingBuffer.memory.vkMemory);

        // copy staging buffer to dst
        const VkCommandBuffer copyCmdBuffer = allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, transferResources.transientCmdPool);
        beginCommandBuffer(copyCmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        // copy command
        VkBufferCopy region = {};
        region.srcOffset = 0;
        region.dstOffset = currentMemoryOffset;
        region.size = copySize;
        vkCmdCopyBuffer(copyCmdBuffer, stagingBuffer.vulkanHandle, target.vulkanHandle, 1, &region);
        endCommandBufferRecording(copyCmdBuffer);

        // submit and wait
        const VkFence fence = submitOneTimeUseCmdBuffer(copyCmdBuffer, vkContext.transferQueue);
        waitForFence(fence);

        // cleanup
        vkDestroyFence(vkContext.device, fence, nullptr);
        vkFreeCommandBuffers(vkContext.device, transferResources.transientCmdPool, 1, &copyCmdBuffer);
    }
}