#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

VkSwapchainKHR createVulkanSwapChain(const int minImageCount, const VkSurfaceKHR surface, const VkSurfaceFormatKHR format);