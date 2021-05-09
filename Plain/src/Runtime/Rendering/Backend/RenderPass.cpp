#include "pch.h"
#include "RenderPass.h"
#include "VulkanImageFormats.h"
#include "VulkanContext.h"

VkRenderPass createVulkanRenderPass(const std::vector<Attachment>& attachments) {

    VkRenderPass            pass;
    VkRenderPassCreateInfo  info;
    std::vector<VkAttachmentDescription> descriptions;
    VkSubpassDescription subpass;
    std::vector<VkAttachmentReference> colorReferences;
    VkAttachmentReference  depthReference;
    bool hasDepth = false;

    //attachment descriptions
    for (uint32_t i = 0; i < attachments.size(); i++) {

        Attachment attachment = attachments[i];
        VkAttachmentLoadOp loadOp;
        VkAttachmentDescription desc;

        switch (attachment.loadOp) {
        case(AttachmentLoadOp::Clear): loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; break;
        case(AttachmentLoadOp::Load): loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; break;
        case(AttachmentLoadOp::DontCare): loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; break;
        default: throw std::runtime_error("Unknown attachment load op");
        }

        VkImageLayout layout = isDepthFormat(attachment.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        desc.flags = 0;
        desc.format = imageFormatToVulkanFormat(attachment.format);
        desc.samples = VK_SAMPLE_COUNT_1_BIT;
        desc.loadOp = loadOp;
        desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc.initialLayout = layout;
        desc.finalLayout = layout;

        VkAttachmentReference ref;
        ref.attachment = i;
        ref.layout = desc.initialLayout;
        if (desc.format == VK_FORMAT_D16_UNORM ||
            desc.format == VK_FORMAT_D32_SFLOAT) {
            depthReference = ref;
            hasDepth = true;
        }
        else {
            colorReferences.push_back(ref);
        }
        descriptions.push_back(desc);
    }

    //subpass
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = (uint32_t)colorReferences.size();
    subpass.pColorAttachments = colorReferences.data();
    subpass.pResolveAttachments = nullptr;
    subpass.pDepthStencilAttachment = hasDepth ? &depthReference : nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;

    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.attachmentCount = (uint32_t)descriptions.size();
    info.pAttachments = descriptions.data();
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 0;
    info.pDependencies = nullptr;

    auto res = vkCreateRenderPass(vkContext.device, &info, nullptr, &pass);
    checkVulkanResult(res);

    return pass;
}