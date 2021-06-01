#include "pch.h"
#include "VulkanSwapchain.h"
#include "VulkanContext.h"
#include "VulkanSurface.h"

VkSwapchainKHR createVulkanSwapChain(const int minImageCount, const VkSurfaceKHR surface, const VkSurfaceFormatKHR format) {

    const VkSurfaceCapabilitiesKHR surfaceCapabilities = getSurfaceCapabilities(surface);

    VkSwapchainCreateInfoKHR swapchainInfo;
    swapchainInfo.sType             = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.pNext             = nullptr;
    swapchainInfo.flags             = 0;
    swapchainInfo.surface           = surface;
    swapchainInfo.minImageCount     = minImageCount;
    swapchainInfo.imageFormat       = format.format;
    swapchainInfo.imageColorSpace   = format.colorSpace;
    swapchainInfo.imageExtent       = surfaceCapabilities.currentExtent;
    swapchainInfo.imageArrayLayers  = 1;
    swapchainInfo.imageUsage        = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    swapchainInfo.preTransform      = surfaceCapabilities.currentTransform;
    swapchainInfo.compositeAlpha    = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode       = VK_PRESENT_MODE_FIFO_KHR;
    swapchainInfo.clipped           = VK_FALSE;
    swapchainInfo.oldSwapchain      = VK_NULL_HANDLE;

    uint32_t uniqueFamilies[2] = { 
        vkContext.queueFamilies.graphics, 
        vkContext.queueFamilies.presentation };

    const bool sharingQueueFamilies = uniqueFamilies[0] == uniqueFamilies[1];
    if (sharingQueueFamilies) {
        swapchainInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        swapchainInfo.queueFamilyIndexCount = 2;
        swapchainInfo.pQueueFamilyIndices   = uniqueFamilies;
    }
    else {
        swapchainInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.queueFamilyIndexCount = 0;
        swapchainInfo.pQueueFamilyIndices   = nullptr;
    }

    VkSwapchainKHR swapchain;
    const auto result = vkCreateSwapchainKHR(vkContext.device, &swapchainInfo, nullptr, &swapchain);
    checkVulkanResult(result);
    return swapchain;
}