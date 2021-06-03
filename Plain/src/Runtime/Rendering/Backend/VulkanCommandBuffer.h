#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

VkCommandBuffer                 allocateCommandBuffer(const VkCommandBufferLevel level, const VkCommandPool& pool);

// returned fence must be manually destroyed
VkFence                         submitOneTimeUseCmdBuffer(const VkCommandBuffer cmdBuffer, const VkQueue queue);

// allocates one command buffer for every pool for every frame
std::vector<VkCommandBuffer>    createGraphicPassMeshCommandBuffers(
    const std::vector<VkCommandPool> &pools, 
    const int frameCounts);