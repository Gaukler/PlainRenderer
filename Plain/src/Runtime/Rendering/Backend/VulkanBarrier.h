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

// multiple barriers may be needed, as mip levels may have differing layouts
// sets image.layout to newLayout, image.currentAccess to dstAccess and image.currentlyWriting to false
std::vector<VkImageMemoryBarrier> createImageBarriers(
    Image&              image,
    const VkImageLayout newLayout,
    const VkAccessFlags dstAccess,
    const uint32_t      baseMip,
    const uint32_t      mipLevels);