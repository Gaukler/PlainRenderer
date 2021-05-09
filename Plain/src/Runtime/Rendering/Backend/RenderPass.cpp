#include "pch.h"
#include "RenderPass.h"
#include "VulkanImageFormats.h"
#include "VulkanContext.h"

RenderPassType getRenderPassType(const RenderPassHandle handle) {
    // first bit indicates pass type
    const bool firstBitSet = bool(handle.index >> 31);
    if (firstBitSet) {
        return RenderPassType::Graphic;
    }
    else {
        return RenderPassType::Compute;
    }
}

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

std::vector<VkClearValue> createGraphicPassClearValues(const std::vector<Attachment>& attachments) {
    std::vector<VkClearValue> clearValues;
    for (const auto& attachment : attachments) {
        if (!isDepthFormat(attachment.format)) {
            VkClearValue colorClear = {};
            colorClear.color = { 0, 0, 0, 0 };
            clearValues.push_back(colorClear);
        }
        else {
            VkClearValue depthClear = {};
            depthClear.depthStencil.depth = 0.f;
            clearValues.push_back(depthClear);
        }
    }
    return clearValues;
}

std::vector<VkPipelineColorBlendAttachmentState> createAttachmentBlendStates(const std::vector<Attachment>& attachments, 
    const BlendState blendState) {
    // only global blending state for all attachments
    // currently only no blending and additive supported
    VkPipelineColorBlendAttachmentState attachmentBlendState;
    attachmentBlendState.blendEnable = blendState != BlendState::None ? VK_TRUE : VK_FALSE;
    attachmentBlendState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    attachmentBlendState.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    attachmentBlendState.colorBlendOp = VK_BLEND_OP_ADD;
    attachmentBlendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attachmentBlendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attachmentBlendState.alphaBlendOp = VK_BLEND_OP_ADD;
    attachmentBlendState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    std::vector<VkPipelineColorBlendAttachmentState> blendingAttachments;
    for (const auto& attachment : attachments) {
        if (!isDepthFormat(attachment.format)) {
            blendingAttachments.push_back(attachmentBlendState);
        }
    }
    return blendingAttachments;
}

VkPipelineColorBlendStateCreateInfo createBlendState(
    const std::vector<VkPipelineColorBlendAttachmentState>& blendAttachmentStates) {
    VkPipelineColorBlendStateCreateInfo blending;
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.pNext = nullptr;
    blending.flags = 0;
    blending.logicOpEnable = VK_FALSE;
    blending.logicOp = VK_LOGIC_OP_NO_OP;
    blending.attachmentCount = (uint32_t)blendAttachmentStates.size();
    blending.pAttachments = blendAttachmentStates.data();
    blending.blendConstants[0] = 0.f;
    blending.blendConstants[1] = 0.f;
    blending.blendConstants[2] = 0.f;
    blending.blendConstants[3] = 0.f;
    return blending;
}

void destroyGraphicPass(const GraphicPass& pass) {
    vkDestroyRenderPass(vkContext.device, pass.vulkanRenderPass, nullptr);
    vkDestroyPipelineLayout(vkContext.device, pass.pipelineLayout, nullptr);
    vkDestroyPipeline(vkContext.device, pass.pipeline, nullptr);
    vkDestroyDescriptorSetLayout(vkContext.device, pass.descriptorSetLayout, nullptr);
}

void destroyComputePass(const ComputePass& pass) {
    vkDestroyPipelineLayout(vkContext.device, pass.pipelineLayout, nullptr);
    vkDestroyPipeline(vkContext.device, pass.pipeline, nullptr);
    vkDestroyDescriptorSetLayout(vkContext.device, pass.descriptorSetLayout, nullptr);
}