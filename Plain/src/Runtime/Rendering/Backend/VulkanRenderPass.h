#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

VkRenderPassBeginInfo createRenderPassBeginInfo(const uint32_t width, const uint32_t height, const VkRenderPass pass,
    const VkFramebuffer framebuffer, const std::vector<VkClearValue>& clearValues);