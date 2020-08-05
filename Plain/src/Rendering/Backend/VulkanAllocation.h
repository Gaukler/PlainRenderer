#pragma once
#include "pch.h"
#include "vulkan/vulkan.h"

struct VulkanAllocation {
    uint32_t        poolIndex;
    VkDeviceMemory  vkMemory = VK_NULL_HANDLE;
    VkDeviceSize    offset;
    VkDeviceSize    padding; //used to find allocation when freeing
};