#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

VkCommandPool createCommandPool(const uint32_t queueFamilyIndex, const VkCommandPoolCreateFlagBits flags);

// create one pool per job system worker thread
std::vector<VkCommandPool> createDrawcallCommandPools();