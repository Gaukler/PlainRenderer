#include "pch.h"
#include "Buffer.h"
#include "VulkanContext.h"
#include "VkMemoryAllocator.h"
#include "Utilities/GeneralUtils.h"

Buffer createVulkanBuffer(const VkDeviceSize size, const std::vector<uint32_t>& queueFamilies, 
    const VkBufferUsageFlags usage, const uint32_t memoryFlags) {

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.flags = 0;
    bufferInfo.size = size;
    bufferInfo.usage = usage;

    //find unique queue families
    std::vector<uint32_t> uniqueQueueFamilies;
    for (const auto& index : queueFamilies) {
        if (!vectorContains(uniqueQueueFamilies, index)) {
            uniqueQueueFamilies.push_back(index);
        }
    }

    if (queueFamilies.size() > 1) {
        bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
    }
    else {
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    bufferInfo.queueFamilyIndexCount = (uint32_t)uniqueQueueFamilies.size();
    bufferInfo.pQueueFamilyIndices = uniqueQueueFamilies.data();

    Buffer buffer;
    buffer.size = size;
    auto res = vkCreateBuffer(vkContext.device, &bufferInfo, nullptr, &buffer.vulkanHandle);
    assert(res == VK_SUCCESS);

    //memory
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(vkContext.device, buffer.vulkanHandle, &memoryRequirements);
    if (!VkMemoryAllocator::getRef().allocate(memoryRequirements, memoryFlags, &buffer.memory)) {
        throw("Could not allocate buffer memory");
    }
    res = vkBindBufferMemory(vkContext.device, buffer.vulkanHandle, buffer.memory.vkMemory, buffer.memory.offset);
    assert(res == VK_SUCCESS);

    return buffer;
}

void destroyVulkanBuffer(const Buffer& buffer) {
    vkDestroyBuffer(vkContext.device, buffer.vulkanHandle, nullptr);
    VkMemoryAllocator::getRef().free(buffer.memory);
}