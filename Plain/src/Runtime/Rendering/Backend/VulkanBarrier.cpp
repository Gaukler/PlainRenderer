#include "pch.h"
#include "VulkanBarrier.h"
#include "VulkanImage.h"

VkBufferMemoryBarrier createBufferBarrier(const Buffer& buffer, 
    const VkAccessFlagBits srcAccess, const VkAccessFlagBits dstAccess) {

    VkBufferMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = srcAccess;
    barrier.srcAccessMask = dstAccess;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer.vulkanHandle;
    barrier.offset = 0;
    barrier.size = buffer.size;
    return barrier;
}

VkImageMemoryBarrier createImageBarrier(const Image& image, const VkAccessFlags dstAccess, 
    const VkImageLayout newLayout, const size_t mipLevel) {

    VkImageMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = image.currentAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = image.layoutPerMip[mipLevel];
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image.vulkanHandle;
    barrier.subresourceRange.aspectMask = getVkImageAspectFlags(image.format);
    barrier.subresourceRange.baseMipLevel = mipLevel;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = computeImageLayerCount(image.desc.type);
    return barrier;
}

void issueBarriersCommand(const VkCommandBuffer commandBuffer,
    const std::vector<VkImageMemoryBarrier>& imageBarriers,
    const std::vector<VkBufferMemoryBarrier>& memoryBarriers) {

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr,
        (uint32_t)memoryBarriers.size(), memoryBarriers.data(), (uint32_t)imageBarriers.size(), imageBarriers.data());
}

std::vector<VkImageMemoryBarrier> createImageBarriers(
    Image& image,
    const VkImageLayout newLayout,
    const VkAccessFlags dstAccess,
    const uint32_t baseMip,
    const uint32_t mipLevels) {

    std::vector<VkImageMemoryBarrier> barriers;
    const VkImageMemoryBarrier firstBarrier = createImageBarrier(image, dstAccess, newLayout, baseMip);
    barriers.push_back(firstBarrier);

    // add subsequent mip level barriers
    for (uint32_t mipOffset = 1; mipOffset < mipLevels; mipOffset++) {

        uint32_t mipLevel = baseMip + mipOffset;
        const bool canExtendLastBarrier = image.layoutPerMip[mipLevel] == barriers.back().oldLayout;
        if (canExtendLastBarrier) {
            barriers.back().subresourceRange.levelCount++;
        }
        else {
            const VkImageMemoryBarrier barrier = createImageBarrier(image, dstAccess, newLayout, mipLevel);
            barriers.push_back(barrier);
        }
    }

    for (uint32_t i = baseMip; i < baseMip + mipLevels; i++) {
        image.layoutPerMip[i] = newLayout;
    }
    image.currentAccess = dstAccess;
    image.currentlyWriting = false;

    return barriers;
}