#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "../ResourceDescriptions.h"
#include "Resources.h"

// forward declaration
class VkMemoryAllocator;

bool isVulkanDepthFormat(VkFormat format);

VkImageType                 imageTypeToVulkanImageType(const ImageType inType);
VkImageViewType             imageTypeToVulkanImageViewType(const ImageType inType);
uint32_t                    computeImageMipCount(const ImageDescription& desc);
uint32_t                    computeImageLayerCount(const ImageType imageType);
VkImageCreateFlags          getVulkanImageCreateFlags(const ImageType type);
VkImageUsageFlags           getVulkanImageUsageFlags(const ImageDescription& desc, const bool isTransferTarget);
std::vector<VkImageView>    createImageViews(const Image& image, const uint32_t mipCount);
VkImageView                 createImageView(const Image& image, const uint32_t baseMip, const uint32_t mipCount);
VkComponentMapping          createDefaultComponentMapping();
VkImageSubresourceRange     createImageSubresourceRange(const ImageDescription& desc, const uint32_t baseMip, const uint32_t mipCount);
VkExtent3D                  createImageExtent(const ImageDescription& desc);
VkImage                     createVulkanImage(const ImageDescription& desc, const bool isTransferTarget);
std::vector<VkImageLayout>  createInitialImageLayouts(const uint32_t mipCount);
VkImageAspectFlags          getVkImageAspectFlags(const VkFormat format);
void                        destroyImageViews(const std::vector<VkImageView> &imageViews);
VkImageSubresourceLayers    createSubresourceLayers(const Image& image, const uint32_t mipLevel);
VulkanAllocation            allocateAndBindImageMemory(const VkImage image, VkMemoryAllocator* inOutMemoryVulkanAllocator);
void                        generateMipChainImmediate(Image& image, const VkImageLayout newLayout, const VkCommandPool transientCmdPool);
void                        imageLayoutTransitionImmediate(Image& image, const VkImageLayout newLayout, const VkCommandPool transientCmdPool);
void                        recordImageLayoutTransition(Image& image, const VkImageLayout newLayout, const VkCommandBuffer cmdBuffer);