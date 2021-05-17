#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

VkCommandBuffer allocateCommandBuffer(const VkCommandBufferLevel level, const VkCommandPool& pool);