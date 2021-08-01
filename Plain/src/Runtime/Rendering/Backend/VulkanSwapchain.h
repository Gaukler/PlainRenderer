#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "Resources.h"

struct Swapchain {
    VkSurfaceKHR        surface         = VK_NULL_HANDLE;
    VkSurfaceFormatKHR  surfaceFormat   = {};
    VkSwapchainKHR      vulkanHandle;
    uint32_t            minImageCount;
    std::vector<Image>  images;
    VkSemaphore         imageAvailable;
};

VkSwapchainKHR createVulkanSwapchain(
    const int                   minImageCount, 
    const VkSurfaceKHR          surface, 
    const VkSurfaceFormatKHR    format);

std::vector<Image> createSwapchainImages(
    const uint32_t          width,
    const uint32_t          height,
    const VkSwapchainKHR    swapchain,
    const VkFormat          format);

void presentImage(const VkSemaphore waitSemaphore, const VkSwapchainKHR swapchain, const uint32_t imageIndex);
void destroySwapchain(const Swapchain& swapchain);