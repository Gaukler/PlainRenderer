#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "Resources.h"

VkBufferMemoryBarrier createBufferBarrier(const Buffer& buffer,
    const VkAccessFlagBits srcAccess, const VkAccessFlagBits dstAccess);

VkImageMemoryBarrier createImageBarrier(const Image& image, const VkAccessFlags dstAccess,
    const VkImageLayout newLayout, const size_t mipLevel);

void issueBarriersCommand(const VkCommandBuffer commandBuffer,
    const std::vector<VkImageMemoryBarrier>& imageBarriers,
    const std::vector<VkBufferMemoryBarrier>& memoryBarriers);