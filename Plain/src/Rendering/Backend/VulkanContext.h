#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

struct QueueFamilies {
    uint32_t graphicsQueueIndex;
    uint32_t presentationQueueIndex;
    uint32_t computeQueueIndex;
    uint32_t transferQueueFamilyIndex;
};

//singleton storing vulkan context
struct VulkanContext {
    VkInstance vulkanInstance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    QueueFamilies queueFamilies = {};
    VkQueue graphicQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;
};

extern VulkanContext vkContext;