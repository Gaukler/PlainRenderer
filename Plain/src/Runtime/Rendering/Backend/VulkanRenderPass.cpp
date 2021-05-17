#include "pch.h"
#include "VulkanRenderPass.h"

VkRenderPassBeginInfo createRenderPassBeginInfo(const uint32_t width, const uint32_t height, const VkRenderPass pass,
    const VkFramebuffer framebuffer, const std::vector<VkClearValue>& clearValues) {

    VkExtent2D extent = {};
    extent.width = width;
    extent.height = height;

    VkRect2D rect = {};
    rect.extent = extent;
    rect.offset = { 0, 0 };

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.renderPass = pass;
    beginInfo.framebuffer = framebuffer;
    beginInfo.clearValueCount = (uint32_t)clearValues.size();
    beginInfo.pClearValues = clearValues.data();
    beginInfo.renderArea = rect;

    return beginInfo;
}