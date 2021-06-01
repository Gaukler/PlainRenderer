#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "VulkanImageFormats.h"

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