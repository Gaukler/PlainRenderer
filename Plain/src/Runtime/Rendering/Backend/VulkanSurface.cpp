#include "pch.h"
#include "VulkanSurface.h"
#include "VulkanContext.h"
#include <glfw/glfw3.h>

VkSurfaceKHR createSurface(GLFWwindow* window) {
    VkSurfaceKHR surface;
    const auto result = glfwCreateWindowSurface(vkContext.vulkanInstance, window, nullptr, &surface);
    checkVulkanResult(result);
    return surface;
}

VkSurfaceFormatKHR chooseSurfaceFormat(const VkSurfaceKHR surface) {

    const std::vector<VkSurfaceFormatKHR> availableFormats = getSupportedSurfaceFormats(surface);
    const VkSurfaceFormatKHR preferredFormat = getPreferredSurfaceFormat();

    const bool isPreferredFormatAvailable = doSupportedSurfaceFormatsContainPreferred(availableFormats, preferredFormat);

    if (isPreferredFormatAvailable) {
        return preferredFormat;
    }
    else {
        std::cerr << "Warning: did not find the requested image format for swapchain" << std::endl;
        VkSurfaceFormatKHR chosenFormat = availableFormats[0]; // vulkan guarantess at least one format
    }
}

std::vector<VkSurfaceFormatKHR> getSupportedSurfaceFormats(const VkSurfaceKHR surface) {
    uint32_t avaibleFormatCount;
    const auto countResult = vkGetPhysicalDeviceSurfaceFormatsKHR(vkContext.physicalDevice, surface, &avaibleFormatCount, nullptr);
    checkVulkanResult(countResult);

    std::vector<VkSurfaceFormatKHR> availableFormats(avaibleFormatCount);
    const auto formatResult = vkGetPhysicalDeviceSurfaceFormatsKHR(vkContext.physicalDevice, surface, &avaibleFormatCount, availableFormats.data());
    checkVulkanResult(formatResult);

    return availableFormats;
}

VkSurfaceFormatKHR getPreferredSurfaceFormat() {
    VkSurfaceFormatKHR requestedFormat;
    requestedFormat.format      = VK_FORMAT_B8G8R8A8_UNORM; // not srgb because it doesn't support image storage
    requestedFormat.colorSpace  = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    return requestedFormat;
}

bool doSupportedSurfaceFormatsContainPreferred(const std::vector<VkSurfaceFormatKHR>& availableFormats, 
    const VkSurfaceFormatKHR& preferred) {
    for (const auto& available : availableFormats) {
        const bool formatsMatch = available.colorSpace == preferred.colorSpace && available.format == preferred.format;
        if (formatsMatch) {
            return true;
        }
    }
    return false;
}

VkSurfaceCapabilitiesKHR getSurfaceCapabilities(const VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    const auto result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkContext.physicalDevice, surface, &surfaceCapabilities);
    checkVulkanResult(result);
    return surfaceCapabilities;
}