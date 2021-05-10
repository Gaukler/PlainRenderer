#include "pch.h"
#include "VulkanCommandRecording.h"
#include "VulkanContext.h"

void resetCommandBuffer(const VkCommandBuffer buffer) {
    auto res = vkResetCommandBuffer(buffer, 0);
    checkVulkanResult(res);
}

void beginCommandBuffer(const VkCommandBuffer buffer, const VkCommandBufferUsageFlags usageFlags,
    const VkCommandBufferInheritanceInfo* pInheritance) {
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = usageFlags;
    beginInfo.pInheritanceInfo = pInheritance;

    const auto res = vkBeginCommandBuffer(buffer, &beginInfo);
    checkVulkanResult(res);
}

VkCommandBufferInheritanceInfo createCommandBufferInheritanceInfo(const VkRenderPass renderPass,
    const VkFramebuffer framebuffer) {
    VkCommandBufferInheritanceInfo inheritanceInfo;
    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritanceInfo.pNext = nullptr;
    inheritanceInfo.renderPass = renderPass;
    inheritanceInfo.subpass = 0;
    inheritanceInfo.framebuffer = framebuffer;
    inheritanceInfo.occlusionQueryEnable = false;
    inheritanceInfo.queryFlags = 0;
    inheritanceInfo.pipelineStatistics = 0;
    return inheritanceInfo;
}

void setCommandBufferViewport(const VkCommandBuffer buffer, const float width, const float height) {
    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(buffer, 0, 1, &viewport);
}

void setCommandBufferScissor(const VkCommandBuffer buffer, const float width, const float height) {
    VkRect2D scissor;
    scissor.offset = { 0, 0 };
    scissor.extent.width = width;
    scissor.extent.height = height;
    vkCmdSetScissor(buffer, 0, 1, &scissor);
}