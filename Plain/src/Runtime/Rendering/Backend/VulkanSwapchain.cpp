#include "pch.h"
#include "VulkanSwapchain.h"
#include "VulkanContext.h"
#include "VulkanSurface.h"
#include "VulkanImage.h"

VkSwapchainKHR createVulkanSwapchain(const int minImageCount, const VkSurfaceKHR surface, const VkSurfaceFormatKHR format) {

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

std::vector<Image> createSwapchainImages(
    const uint32_t          width,
    const uint32_t          height,
    const VkSwapchainKHR    swapchain,
    const VkFormat          format) {

    uint32_t swapchainImageCount = 0;
    if (vkGetSwapchainImagesKHR(vkContext.device, swapchain, &swapchainImageCount, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("failed to query swapchain image count");
    }
    std::vector<VkImage> swapchainImages;
    swapchainImages.resize(swapchainImageCount);
    if (vkGetSwapchainImagesKHR(vkContext.device, swapchain, &swapchainImageCount, swapchainImages.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to query swapchain images");
    }

    std::vector<Image> images;
    for (const auto vulkanImage : swapchainImages) {
        Image image;
        image.vulkanHandle  = vulkanImage;
        image.desc.width    = width;
        image.desc.height   = height;
        image.desc.depth    = 1;
        image.format        = format;
        image.desc.type     = ImageType::Type2D;
        image.viewPerMip.push_back(createImageView(image, 0, 1));
        image.layoutPerMip.push_back(VK_IMAGE_LAYOUT_UNDEFINED);

        images.push_back(image);
    }
    return images;
}

void presentImage(const VkSemaphore waitSemaphore, const VkSwapchainKHR swapchain, const uint32_t imageIndex) {

    VkPresentInfoKHR present;
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pNext = nullptr;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &waitSemaphore;
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain;
    present.pImageIndices = &imageIndex;

    VkResult presentResult;
    present.pResults = &presentResult;

    const VkResult result = vkQueuePresentKHR(vkContext.presentQueue, &present);
    checkVulkanResult(result);
    checkVulkanResult(presentResult);
}

void destroySwapchain(const Swapchain& swapchain) {
    for (const Image& image : swapchain.images) {
        destroyImageViews(image.viewPerMip);
    }
    vkDestroySwapchainKHR(vkContext.device, swapchain.vulkanHandle, nullptr);
    vkDestroySurfaceKHR(vkContext.vulkanInstance, swapchain.surface, nullptr);
}