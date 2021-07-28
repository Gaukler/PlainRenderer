#include "pch.h"
#include "VulkanCommandBuffer.h"
#include "VulkanContext.h"
#include "VulkanSync.h"

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

VkFence submitOneTimeUseCmdBuffer(const VkCommandBuffer cmdBuffer, const VkQueue queue) {

    VkSubmitInfo submit;
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext                = nullptr;
    submit.waitSemaphoreCount   = 0;
    submit.pWaitSemaphores      = nullptr;
    submit.pWaitDstStageMask    = nullptr;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmdBuffer;
    submit.signalSemaphoreCount = 0;
    submit.pSignalSemaphores    = nullptr;

    const VkFence fence = createFence();
    resetFence(fence);

    const auto result = vkQueueSubmit(queue, 1, &submit, fence);
    checkVulkanResult(result);

    return fence;
}

std::vector<VkCommandBuffer> createGraphicPassMeshCommandBuffers(const std::vector<VkCommandPool> &pools, const int frameCounts) {
    std::vector<VkCommandBuffer> buffers;
    for (int frameIndex = 0; frameIndex < frameCounts; frameIndex++) {
        for (int poolIndex = 0; poolIndex < pools.size(); poolIndex++) {
            const VkCommandPool pool = pools[poolIndex];
            buffers.push_back(allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY, pool));
        }
    }
    return buffers;
}

void submitCmdBufferToGraphicsQueue(
    const VkCommandBuffer           commandBuffer,
    const std::vector<VkSemaphore>& waitSemaphores,
    const std::vector<VkSemaphore>& signalSemaphore,
    const VkFence                   submitFence) {

    VkSubmitInfo submit;
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = nullptr;
    submit.waitSemaphoreCount = waitSemaphores.size();
    submit.pWaitSemaphores = waitSemaphores.data();

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit.pWaitDstStageMask = &waitStage;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffer;
    submit.signalSemaphoreCount = signalSemaphore.size();
    submit.pSignalSemaphores = signalSemaphore.data();

    const auto result = vkQueueSubmit(vkContext.graphicQueue, 1, &submit, submitFence);
    checkVulkanResult(result);
}