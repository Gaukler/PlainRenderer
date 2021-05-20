#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

VkFramebuffer createVulkanFramebuffer(const std::vector<VkImageView>& views, const VkRenderPass renderpass,
    const uint32_t width, const uint32_t height);