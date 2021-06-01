#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

VkCommandBuffer allocateCommandBuffer(const VkCommandBufferLevel level, const VkCommandPool& pool);

// returned fence must be manually destroyed
VkFence         submitOneTimeUseCmdBuffer(const VkCommandBuffer cmdBuffer, const VkQueue queue);