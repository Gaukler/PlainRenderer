#pragma once
#include "pch.h"
#include "Vulkan/vulkan.h"

struct GLFWwindow;

VkSurfaceKHR createSurface(GLFWwindow* window);
VkSurfaceFormatKHR chooseSurfaceFormat(const VkSurfaceKHR surface);
std::vector<VkSurfaceFormatKHR> getSupportedSurfaceFormats(const VkSurfaceKHR surface);
VkSurfaceFormatKHR getPreferredSurfaceFormat();
bool doSupportedSurfaceFormatsContainPreferred(const std::vector<VkSurfaceFormatKHR>& availableFormats,
    const VkSurfaceFormatKHR& preferred);
VkSurfaceCapabilitiesKHR getSurfaceCapabilities(const VkSurfaceKHR surface);