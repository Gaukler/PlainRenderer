#include "VulkanFramebuffer.h"
#include "VulkanContext.h"

VkFramebuffer createVulkanFramebuffer(const std::vector<VkImageView>& views, const VkRenderPass renderpass,
    const uint32_t width, const uint32_t height) {

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.pNext = nullptr;
    framebufferInfo.flags = 0;
    framebufferInfo.renderPass = renderpass;
    framebufferInfo.attachmentCount = (uint32_t)views.size();
    framebufferInfo.pAttachments = views.data();
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;

    VkFramebuffer framebuffer;
    const auto result = vkCreateFramebuffer(vkContext.device, &framebufferInfo, nullptr, &framebuffer);
    checkVulkanResult(result);

    return framebuffer;
}