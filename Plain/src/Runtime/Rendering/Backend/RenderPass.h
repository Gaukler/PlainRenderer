#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "Resources.h"
#include "Runtime/Rendering/ResourceDescriptions.h"

enum class RenderPassType { Graphic, Compute };
RenderPassType getRenderPassType(const RenderPassHandle handle);

std::vector<VkClearValue> createGraphicPassClearValues(const std::vector<Attachment>& attachments);
std::vector<VkPipelineColorBlendAttachmentState> createAttachmentBlendStates(
    const std::vector<Attachment>& attachments, const BlendState blendState);
VkPipelineColorBlendStateCreateInfo createBlendState(
    const std::vector<VkPipelineColorBlendAttachmentState>& blendAttachmentStates);

VkRenderPass createVulkanRenderPass(const std::vector<Attachment>& attachments);
void destroyGraphicPass(const GraphicPass& pass);
void destroyComputePass(const ComputePass& pass);