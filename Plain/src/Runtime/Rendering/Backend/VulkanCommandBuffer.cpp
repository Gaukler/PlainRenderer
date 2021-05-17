#include "pch.h"
#include "VulkanCommandBuffer.h"
#include "VulkanContext.h"

VkCommandBuffer allocateCommandBuffer(const VkCommandBufferLevel level, const VkCommandPool& pool) {

    VkCommandBufferAllocateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.commandPool = pool;
    bufferInfo.level = level;
    bufferInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    auto res = vkAllocateCommandBuffers(vkContext.device, &bufferInfo, &commandBuffer);
    checkVulkanResult(res);

    return commandBuffer;
}