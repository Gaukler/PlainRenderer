#include "pch.h"
#include "VulkanImageTransfer.h"
#include "VulkanContext.h"
#include "VulkanCommandBuffer.h"
#include "VulkanCommandRecording.h"
#include "VulkanBarrier.h"
#include "VulkanImage.h"
#include "VulkanSync.h"
#include "VulkanBuffer.h"

// ---- local helper declaration ----

VkDeviceSize    computeImageMipByteSize(const VkExtent3D& extent, const float bytePerPixel);
VkExtent3D      computeNextLowerMipExtent(const VkExtent3D& inExtent);
VkDeviceSize    computeMipRowByteSize(const VkDeviceSize mipWidth, const float bytePerPixel);
VkExtent3D      getMinImageExtentFromFormat(const ImageFormat format);
VkDeviceSize    getImageFormatMinCopySizeInByte(const ImageFormat format);

VkDeviceSize computeNumberOfImageRowsToCopy(
    const VkDeviceSize mipSize,
    const VkDeviceSize offsetFromMipStart,
    const VkDeviceSize stagingBufferSize,
    const VkDeviceSize bytePerRow);

VkDeviceSize computeImageCopySize(
    const VkDeviceSize numberOfRowsToCopy,
    const VkDeviceSize bytesPerRow,
    const VkDeviceSize minCopySize);

VkBufferImageCopy createBufferImageCopyRegion(
    const VkImageSubresourceLayers& subresource,
    const VkOffset3D& offset,
    const VkExtent3D& extent);

VkOffset3D createImageCopyOffset(const VkDeviceSize byteOffsetMip, const VkDeviceSize bytesPerRow);
VkExtent3D createImageCopyExtent(const VkExtent3D  mipExtent, const VkDeviceSize numberOfRowsToCopy);

void transferDataIntoImageImmediate(Image& target, const Data& data, TransferResources& transferResources) {

    const Buffer& stagingBuffer = transferResources.stagingBuffer;

    const float         bytePerPixel = getImageFormatBytePerPixel(target.desc.format);
    const VkDeviceSize  minCopySize = getImageFormatMinCopySizeInByte(target.desc.format);

    uint32_t    mipLevel = 0;
    VkExtent3D  mipExtent;
    mipExtent.width = target.desc.width;
    mipExtent.height = target.desc.height;
    mipExtent.depth = target.desc.depth;

    VkDeviceSize bytesPerRow = computeMipRowByteSize(mipExtent.width, bytePerPixel);
    VkDeviceSize currentMipSize = computeImageMipByteSize(mipExtent, bytePerPixel);

    VkDeviceSize byteOffsetMip = 0;
    VkDeviceSize byteOffsetGlobal = 0;

    const VkCommandBuffer cmdBuffer = allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, transferResources.transientCmdPool);

    // TODO:    staging buffer is always filled as much as is needed to transfer current mip level
    //          would be more efficient to fill staging buffer completely and do multiple mips at once where possible
    while (byteOffsetGlobal < data.size) {

        const bool mipBorderReached = byteOffsetMip >= currentMipSize;
        if (mipBorderReached) {
            mipLevel++;
            byteOffsetMip = 0;

            mipExtent = computeNextLowerMipExtent(mipExtent);
            bytesPerRow = computeMipRowByteSize(mipExtent.width, bytePerPixel);;
            currentMipSize = computeImageMipByteSize(mipExtent, bytePerPixel);
        }

        const VkDeviceSize numberOfRowsToCopy = computeNumberOfImageRowsToCopy(currentMipSize, byteOffsetMip, stagingBuffer.size, bytesPerRow);
        const VkDeviceSize copySize = computeImageCopySize(numberOfRowsToCopy, bytesPerRow, minCopySize);

        fillHostVisibleCoherentBuffer(stagingBuffer, (char*)data.ptr + byteOffsetGlobal, copySize);

        const VkImageSubresourceLayers  subresource = createSubresourceLayers(target, mipLevel);
        const VkOffset3D                offset = createImageCopyOffset(byteOffsetMip, bytesPerRow);
        const VkExtent3D                extent = createImageCopyExtent(mipExtent, numberOfRowsToCopy);

        const VkBufferImageCopy region = createBufferImageCopyRegion(subresource, offset, extent);

        beginCommandBuffer(cmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        const bool isFirstLoop = byteOffsetGlobal == 0;
        if (isFirstLoop) {
            // bring image into proper layout
            const auto toTransferDstBarrier = createImageBarriers(
                target,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_TRANSFER_READ_BIT,
                0,
                (uint32_t)target.viewPerMip.size());

            issueBarriersCommand(cmdBuffer, toTransferDstBarrier, {});
        }

        vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer.vulkanHandle, target.vulkanHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        endCommandBufferRecording(cmdBuffer);
        const VkFence fence = submitOneTimeUseCmdBuffer(cmdBuffer, vkContext.transferQueue);
        waitForFence(fence);
        vkDestroyFence(vkContext.device, fence, nullptr);
        resetCommandBuffer(cmdBuffer);

        byteOffsetMip += copySize;
        byteOffsetGlobal += copySize;
    }

    vkFreeCommandBuffers(vkContext.device, transferResources.transientCmdPool, 1, &cmdBuffer);
}

// ---- local helper implementation ----

VkDeviceSize computeImageMipByteSize(const VkExtent3D& extent, const float bytePerPixel) {
    const VkDeviceSize mipPixelCount = extent.width * extent.height * extent.depth;
    return mipPixelCount * bytePerPixel;
}

VkExtent3D computeNextLowerMipExtent(const VkExtent3D& inExtent) {
    VkExtent3D lowerExtent;
    lowerExtent.width   = glm::max(inExtent.width / 2, (uint32_t)1);
    lowerExtent.height  = glm::max(inExtent.height / 2, (uint32_t)1);
    lowerExtent.depth   = glm::max(inExtent.depth / 2, (uint32_t)1);
    return lowerExtent;
}

VkDeviceSize computeMipRowByteSize(const VkDeviceSize mipWidth, const float bytePerPixel) {
    return (VkDeviceSize)(mipWidth * bytePerPixel);
}

VkExtent3D getMinImageExtentFromFormat(const ImageFormat format) {
    const bool isBCnCompressed = getImageFormatIsBCnCompressed(format);
    VkExtent3D minExtent;
    if (isBCnCompressed) {
        minExtent.width     = 4;
        minExtent.height    = 4;
        minExtent.depth     = 1;
    }
    else {
        minExtent.width     = 1;
        minExtent.height    = 1;
        minExtent.depth     = 1;
    }
    return minExtent;
}

VkDeviceSize getImageFormatMinCopySizeInByte(const ImageFormat format) {
    const bool  isBCnCompressed = getImageFormatIsBCnCompressed(format);
    const float bytePerPixel = getImageFormatBytePerPixel(format);
    if (isBCnCompressed) {
        return (VkDeviceSize)(4 * 4 * bytePerPixel);
    }
    else {
        return 1;
    }
}

VkDeviceSize computeNumberOfImageRowsToCopy(
    const VkDeviceSize mipSize,
    const VkDeviceSize offsetFromMipStart,
    const VkDeviceSize stagingBufferSize,
    const VkDeviceSize bytePerRow) {

    const VkDeviceSize  remainingBytesInMip = mipSize - offsetFromMipStart;
    const VkDeviceSize  copySizeUnrounded = std::min(stagingBufferSize, remainingBytesInMip);
    return copySizeUnrounded / bytePerRow;
}

VkDeviceSize computeImageCopySize(const VkDeviceSize numberOfRowsToCopy, const VkDeviceSize bytesPerRow, const VkDeviceSize minCopySize) {
    VkDeviceSize  copySize = numberOfRowsToCopy * bytesPerRow;
    return glm::max(copySize, minCopySize);
}

VkBufferImageCopy createBufferImageCopyRegion(
    const VkImageSubresourceLayers& subresource,
    const VkOffset3D& offset,
    const VkExtent3D& extent) {

    VkBufferImageCopy region;
    region.bufferOffset         = 0;
    region.imageSubresource     = subresource;
    region.imageOffset          = offset;
    region.bufferRowLength      = 0;            // zero means default values
    region.bufferImageHeight    = 0;            // zero means default values
    region.imageExtent          = extent;
    return region;
}

VkOffset3D createImageCopyOffset(const VkDeviceSize byteOffsetMip, const VkDeviceSize bytesPerRow) {
    VkOffset3D offset;
    offset.x = 0;
    offset.y = byteOffsetMip / bytesPerRow;
    offset.z = 0;
    return offset;
}

VkExtent3D createImageCopyExtent(const VkExtent3D  mipExtent, const VkDeviceSize numberOfRowsToCopy) {
    VkExtent3D extent;
    extent.height   = numberOfRowsToCopy / mipExtent.depth;
    extent.width    = mipExtent.width;
    extent.depth    = mipExtent.depth;
    return extent;
}