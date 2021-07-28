#include "pch.h"
#include "VulkanImage.h"
#include "VulkanContext.h"
#include "Common/Utilities/MathUtils.h"
#include "VulkanImageFormats.h"
#include "VkMemoryAllocator.h"
#include "VulkanCommandRecording.h"
#include "VulkanCommandBuffer.h"
#include "VulkanBarrier.h"
#include "VulkanSync.h"

bool isVulkanDepthFormat(VkFormat format) {
    return (
        format == VK_FORMAT_D16_UNORM ||
        format == VK_FORMAT_D16_UNORM_S8_UINT ||
        format == VK_FORMAT_D24_UNORM_S8_UINT ||
        format == VK_FORMAT_D32_SFLOAT ||
        format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

VkImageType imageTypeToVulkanImageType(const ImageType inType) {
    switch (inType) {
        case ImageType::Type1D:     return VK_IMAGE_TYPE_1D;
        case ImageType::Type2D:     return VK_IMAGE_TYPE_2D;
        case ImageType::Type3D:     return VK_IMAGE_TYPE_3D;
        case ImageType::TypeCube:   return VK_IMAGE_TYPE_2D;
        default: throw std::runtime_error("Unsuported type enum");
    }
}

VkImageViewType imageTypeToVulkanImageViewType(const ImageType inType) {
    switch (inType) {
        case ImageType::Type1D:     return VK_IMAGE_VIEW_TYPE_1D;
        case ImageType::Type2D:     return VK_IMAGE_VIEW_TYPE_2D;
        case ImageType::Type3D:     return VK_IMAGE_VIEW_TYPE_3D;
        case ImageType::TypeCube:   return VK_IMAGE_VIEW_TYPE_CUBE;
        default: throw std::runtime_error("Unsuported type enum");
    }
}


uint32_t computeImageMipCount(const ImageDescription& desc) {
    uint32_t mipCount;
    switch (desc.mipCount) {
    case(MipCount::One): mipCount = 1; break;
    case(MipCount::Manual): mipCount = desc.manualMipCount; break;
    case(MipCount::FullChain): mipCount = mipCountFromResolution(desc.width, desc.height, desc.depth); break;
    default: throw std::runtime_error("Unsuported mipCoun enum");
    }
    return mipCount;
}

uint32_t computeImageLayerCount(const ImageType imageType) {
    if (imageType == ImageType::TypeCube) {
        return 6;
    }
    else {
        return 1;
    }
}

VkImageCreateFlags getVulkanImageCreateFlags(const ImageType type) {
    if (type == ImageType::TypeCube) {
        return VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }
    else {
        return 0;
    }
}

VkImageUsageFlags getVulkanImageUsageFlags(const ImageDescription& desc, const bool isTransferTarget){
    VkImageUsageFlags usage = 0;
    if (bool(desc.usageFlags & ImageUsageFlags::Attachment)) {
        const VkImageUsageFlagBits attachmentUsage = isDepthFormat(desc.format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        usage |= attachmentUsage;
    }
    if (bool(desc.usageFlags & ImageUsageFlags::Sampled)) {
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (bool(desc.usageFlags & ImageUsageFlags::Storage)) {
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (isTransferTarget) {
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    if (desc.autoCreateMips) {
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    return usage;
}

std::vector<VkImageView> createImageViews(const Image& image, const uint32_t mipCount) {
    std::vector<VkImageView> views;
    views.reserve(mipCount);
    for (uint32_t mipLevel = 0; mipLevel < mipCount; mipLevel++) {
        const uint32_t remainingMipCount = mipCount - mipLevel;
        const VkImageView view = createImageView(image, mipLevel, remainingMipCount);
        views.push_back(view);
    }
    return views;
}

VkImageView createImageView(const Image& image, const uint32_t baseMip, const uint32_t mipCount) {

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.pNext = nullptr;
    viewInfo.flags = 0;
    viewInfo.image = image.vulkanHandle;
    viewInfo.viewType = imageTypeToVulkanImageViewType(image.desc.type);
    viewInfo.format = image.format;
    viewInfo.components = createDefaultComponentMapping();
    viewInfo.subresourceRange = createImageSubresourceRange(image.desc, baseMip, mipCount);

    VkImageView view;
    const auto res = vkCreateImageView(vkContext.device, &viewInfo, nullptr, &view);
    checkVulkanResult(res);

    return view;
}

VkComponentMapping createDefaultComponentMapping() {
    VkComponentMapping components;
    components.r = VK_COMPONENT_SWIZZLE_R;
    components.g = VK_COMPONENT_SWIZZLE_G;
    components.b = VK_COMPONENT_SWIZZLE_B;
    components.a = VK_COMPONENT_SWIZZLE_A;
    return components;
}

VkImageSubresourceRange createImageSubresourceRange(const ImageDescription& desc, const uint32_t baseMip, const uint32_t mipCount) {
    VkImageSubresourceRange subresource = {};
    subresource.aspectMask = imageFormatToVkAspectFlagBits(desc.format);
    subresource.baseMipLevel = baseMip;
    subresource.levelCount = mipCount;
    subresource.baseArrayLayer = 0;
    subresource.layerCount = computeImageLayerCount(desc.type);
    return subresource;
}

VkExtent3D createImageExtent(const ImageDescription& desc) {
    VkExtent3D extent;
    extent.width = desc.width;
    extent.height = desc.height;
    extent.depth = desc.depth;
    return extent;
}

VkImage createVulkanImage(const ImageDescription& desc, const bool isTransferTarget) {
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = nullptr;
    imageInfo.flags = getVulkanImageCreateFlags(desc.type);
    imageInfo.imageType = imageTypeToVulkanImageType(desc.type);
    imageInfo.format = imageFormatToVulkanFormat(desc.format);
    imageInfo.extent = createImageExtent(desc);
    imageInfo.mipLevels = computeImageMipCount(desc);
    imageInfo.arrayLayers = computeImageLayerCount(desc.type);
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = getVulkanImageUsageFlags(desc, isTransferTarget);
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.queueFamilyIndexCount = 1;
    imageInfo.pQueueFamilyIndices = &vkContext.queueFamilies.graphics;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image;
    const auto res = vkCreateImage(vkContext.device, &imageInfo, nullptr, &image);
    checkVulkanResult(res);
    return image;
}

std::vector<VkImageLayout> createInitialImageLayouts(const uint32_t mipCount) {
    return std::vector<VkImageLayout>(mipCount, VK_IMAGE_LAYOUT_UNDEFINED);
}

VkImageAspectFlags getVkImageAspectFlags(const VkFormat format) {
    if (isVulkanDepthFormat(format)) {
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    else {
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

void destroyImageViews(const std::vector<VkImageView>& imageViews) {
    for (const VkImageView &view : imageViews) {
        vkDestroyImageView(vkContext.device, view, nullptr);
    }
}

VkImageSubresourceLayers createSubresourceLayers(const Image& image, const uint32_t mipLevel) {
    VkImageSubresourceLayers layers;
    layers.aspectMask = getVkImageAspectFlags(image.format);
    layers.mipLevel = mipLevel;
    layers.baseArrayLayer = 0;
    layers.layerCount = 1;
    return layers;
}

VulkanAllocation allocateAndBindImageMemory(const VkImage image, VkMemoryAllocator* inOutMemoryVulkanAllocator) {
    assert(inOutMemoryVulkanAllocator);
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(vkContext.device, image, &memoryRequirements);

    const VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VulkanAllocation allocation;
    const bool allocationSuccess = inOutMemoryVulkanAllocator->allocate(memoryRequirements, memoryFlags, &allocation);
    if (!allocationSuccess) {
        throw("Could not allocate image memory");
    }

    auto res = vkBindImageMemory(vkContext.device, image, allocation.vkMemory, allocation.offset);
    checkVulkanResult(res);

    return allocation;
}

void generateMipChainImmediate(Image& image, const VkImageLayout newLayout, const VkCommandPool transientCmdPool) {

    // check for linear filtering support
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(vkContext.physicalDevice, image.format, &formatProps);
    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("physical device lacks linear filtering support for used format");
    }

    VkImageBlit blitInfo;

    // offset base stays zero
    blitInfo.srcOffsets[0].x = 0;
    blitInfo.srcOffsets[0].y = 0;
    blitInfo.srcOffsets[0].z = 0;

    blitInfo.dstOffsets[0].x = 0;
    blitInfo.dstOffsets[0].y = 0;
    blitInfo.dstOffsets[0].z = 0;

    // initial offset extent
    blitInfo.srcOffsets[1].x = image.desc.width;
    blitInfo.srcOffsets[1].y = image.desc.height;
    blitInfo.srcOffsets[1].z = image.desc.depth;

    blitInfo.dstOffsets[1].x = blitInfo.srcOffsets[1].x != 1 ? blitInfo.srcOffsets[1].x / 2 : 1;
    blitInfo.dstOffsets[1].y = blitInfo.srcOffsets[1].y != 1 ? blitInfo.srcOffsets[1].y / 2 : 1;
    blitInfo.dstOffsets[1].z = blitInfo.srcOffsets[1].z != 1 ? blitInfo.srcOffsets[1].z / 2 : 1;

    const VkCommandBuffer blitCmdBuffer = allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, transientCmdPool);
    beginCommandBuffer(blitCmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    for (uint32_t srcMip = 0; srcMip < image.viewPerMip.size() - 1; srcMip++) {

        // barriers
        auto barriers = createImageBarriers(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, srcMip, 1); //src
        const auto dstBarriers = createImageBarriers(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, srcMip + 1, 1); //dst
        barriers.insert(barriers.end(), dstBarriers.begin(), dstBarriers.end());

        issueBarriersCommand(blitCmdBuffer, barriers, std::vector<VkBufferMemoryBarrier> {});

        // blit operation
        blitInfo.srcSubresource = createSubresourceLayers(image, srcMip);
        blitInfo.dstSubresource = createSubresourceLayers(image, srcMip + 1);

        vkCmdBlitImage(blitCmdBuffer, image.vulkanHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image.vulkanHandle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitInfo, VK_FILTER_LINEAR);

        // update offsets
        blitInfo.srcOffsets[1].x /= blitInfo.srcOffsets[1].x != 1 ? 2 : 1;
        blitInfo.srcOffsets[1].y /= blitInfo.srcOffsets[1].y != 1 ? 2 : 1;
        blitInfo.srcOffsets[1].z /= blitInfo.srcOffsets[1].z != 1 ? 2 : 1;

        blitInfo.dstOffsets[1].x = blitInfo.srcOffsets[1].x != 1 ? blitInfo.srcOffsets[1].x / 2 : 1;
        blitInfo.dstOffsets[1].y = blitInfo.srcOffsets[1].y != 1 ? blitInfo.srcOffsets[1].y / 2 : 1;
        blitInfo.dstOffsets[1].z = blitInfo.srcOffsets[1].z != 1 ? blitInfo.srcOffsets[1].z / 2 : 1;
    }

    // bring image into new layout
    const auto newLayoutBarriers = createImageBarriers(image, newLayout, VK_ACCESS_TRANSFER_WRITE_BIT, 0, (uint32_t)image.viewPerMip.size());
    issueBarriersCommand(blitCmdBuffer, newLayoutBarriers, std::vector<VkBufferMemoryBarrier> {});

    endCommandBufferRecording(blitCmdBuffer);

    // submit
    const VkFence fence = submitOneTimeUseCmdBuffer(blitCmdBuffer, vkContext.transferQueue);
    waitForFence(fence);

    // cleanup
    vkDestroyFence(vkContext.device, fence, nullptr);
    vkFreeCommandBuffers(vkContext.device, transientCmdPool, 1, &blitCmdBuffer);
}

void imageLayoutTransitionImmediate(Image& image, const VkImageLayout newLayout, const VkCommandPool transientCmdPool) {

    const VkCommandBuffer transitionCmdBuffer = allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, transientCmdPool);
    beginCommandBuffer(transitionCmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    recordImageLayoutTransition(image, newLayout, transitionCmdBuffer);
    endCommandBufferRecording(transitionCmdBuffer);

    const VkFence fence = submitOneTimeUseCmdBuffer(transitionCmdBuffer, vkContext.transferQueue);
    waitForFence(fence);

    vkDestroyFence(vkContext.device, fence, nullptr);
    vkFreeCommandBuffers(vkContext.device, transientCmdPool, 1, &transitionCmdBuffer);
}

void recordImageLayoutTransition(Image& image, const VkImageLayout newLayout, const VkCommandBuffer cmdBuffer) {
    const auto newLayoutBarriers = createImageBarriers(
        image,
        newLayout,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        0,
        (uint32_t)image.viewPerMip.size());
    issueBarriersCommand(cmdBuffer, newLayoutBarriers, std::vector<VkBufferMemoryBarrier> {});
}