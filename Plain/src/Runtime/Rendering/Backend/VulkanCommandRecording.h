#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

void resetCommandBuffer(const VkCommandBuffer buffer);
void beginCommandBuffer(const VkCommandBuffer buffer, const VkCommandBufferUsageFlags usageFlags, 
    const VkCommandBufferInheritanceInfo* pInheritance = nullptr);
void endCommandBufferRecording(const VkCommandBuffer cmdBuffer);
VkCommandBufferInheritanceInfo createCommandBufferInheritanceInfo(const VkRenderPass renderPass,
    const VkFramebuffer framebuffer);
void setCommandBufferViewport(const VkCommandBuffer buffer, const float width, const float height);
void setCommandBufferScissor(const VkCommandBuffer buffer, const float width, const float height);