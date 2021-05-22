#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "Resources.h"

struct Swapchain {
    VkSurfaceKHR        surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR  surfaceFormat = {};
    VkSwapchainKHR      vulkanHandle;
    uint32_t            minImageCount;
    std::vector<Image>  images;
    VkSemaphore         imageAvailable;
};

VkSwapchainKHR createVulkanSwapChain(const int minImageCount, const VkSurfaceKHR surface, const VkSurfaceFormatKHR format);