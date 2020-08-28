#pragma once
#include "pch.h"
#include "vulkan/vulkan.h"

struct VulkanAllocation {
    uint32_t        memoryIndex = 0;
    uint32_t        poolIndex   = 0;
    VkDeviceMemory  vkMemory    = VK_NULL_HANDLE;
    VkDeviceSize    offset      = 0;
    VkDeviceSize    padding     = 0;
};