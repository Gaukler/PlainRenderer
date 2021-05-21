#include "pch.h"
#include "PerFrameResources.h"
#include "VulkanContext.h"
#include "VulkanCommandBuffer.h"
#include "VulkanFramebuffer.h"

PerFrameResources createPerFrameResources(const VkCommandPool cmdPool) {

    const uint32_t timestampQueryPoolQueryCount = 100;

    PerFrameResources perFrameResources;
    perFrameResources.commandBuffer = allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, cmdPool);
    perFrameResources.timestampQueryPool.vulkanHandle = createQueryPool(VK_QUERY_TYPE_TIMESTAMP, timestampQueryPoolQueryCount);
    perFrameResources.timestampQueryPool.queryCount = 0;

    return perFrameResources;
}

void destroyPerFrameResources(PerFrameResources* inOutFrameResources) {
    destroyFramebuffers(inOutFrameResources->transientFramebuffers);
    inOutFrameResources->transientFramebuffers.clear();
    vkDestroyQueryPool(vkContext.device, inOutFrameResources->timestampQueryPool.vulkanHandle, nullptr);
    inOutFrameResources->timestampQueryPool.queryCount = 0;
}