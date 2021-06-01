#include "pch.h"
#include "VulkanImageTransfer.h"

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