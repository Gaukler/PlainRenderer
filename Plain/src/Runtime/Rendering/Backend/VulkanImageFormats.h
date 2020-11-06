#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "Resources.h"

VkFormat imageFormatToVulkanFormat(const ImageFormat format);
VkImageAspectFlagBits imageFormatToVkAspectFlagBits(const ImageFormat format);

ImageFormat vulkanImageFormatToImageFormat(const VkFormat format);