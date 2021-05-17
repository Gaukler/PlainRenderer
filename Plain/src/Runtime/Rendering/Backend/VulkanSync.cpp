#include "pch.h"
#include "VulkanSync.h"
#include <vulkan/vulkan.h>
#include "VulkanContext.h"

VkSemaphore createSemaphore() {

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.flags = 0;

    VkSemaphore semaphore;
    auto res = vkCreateSemaphore(vkContext.device, &semaphoreInfo, nullptr, &semaphore);
    checkVulkanResult(res);

    return semaphore;
}

VkFence createFence() {

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkFence fence;
    const auto result = vkCreateFence(vkContext.device, &fenceInfo, nullptr, &fence);
    checkVulkanResult(result);

    return fence;
}

void waitForFence(const VkFence fence) {
    const auto result = vkWaitForFences(vkContext.device, 1, &fence, true, UINT64_MAX);
    checkVulkanResult(result);
}

void resetFence(const VkFence fence) {
    const auto result = vkResetFences(vkContext.device, 1, &fence);
    checkVulkanResult(result);
}