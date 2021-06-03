#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

struct QueueFamilies {
    uint32_t graphics;
    uint32_t presentation;
    uint32_t compute;
    uint32_t transfer;
};

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

VkDebugReportCallbackEXT setupDebugCallbacks();

// returns true if all family indices have been found, in that case indices are writen to QueueFamilies pointer
bool                        getQueueFamilies(const VkPhysicalDevice device, QueueFamilies* pOutQueueFamilies, const VkSurfaceKHR surface);

void                        checkVulkanResult(const VkResult result);
std::vector<const char*>    getRequiredInstanceExtensions();
void                        createVulkanInstance();
void                        destroyVulkanInstance();
bool                        hasRequiredDeviceFeatures(const VkPhysicalDevice physicalDevice);
void                        pickPhysicalDevice(const VkSurfaceKHR surface);
void                        createLogicalDevice();
VkPhysicalDeviceProperties  getVulkanDeviceProperties();
void                        initializeVulkanQueues();
void                        waitForGpuIdle();

extern VulkanContext vkContext;