#include "pch.h"
#include "VulkanCommandBuffer.h"
#include "VulkanContext.h"
#include "Common/JobSystem.h"

VkCommandPool createCommandPool(const uint32_t queueFamilyIndex, const VkCommandPoolCreateFlagBits flags) {

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = flags;
    poolInfo.queueFamilyIndex = queueFamilyIndex;

    VkCommandPool pool;
    auto res = vkCreateCommandPool(vkContext.device, &poolInfo, nullptr, &pool);
    checkVulkanResult(res);

    return pool;
}

std::vector<VkCommandPool> createDrawcallCommandPools() {
    std::vector<VkCommandPool> drawcallPools;
    for (int i = 0; i < JobSystem::getWorkerCount(); i++) {
        const VkCommandPool pool = createCommandPool(vkContext.queueFamilies.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        drawcallPools.push_back(pool);
    }
    return drawcallPools;
}