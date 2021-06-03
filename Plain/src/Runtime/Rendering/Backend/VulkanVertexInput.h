#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "VertexInput.h"
#include "Runtime/Rendering/ResourceDescriptions.h"

std::vector<VkVertexInputAttributeDescription>  createVertexInputDescriptions(const VertexInputFlags inputFlags);
VkVertexInputBindingDescription                 createVertexInputBindingDescription(const VertexFormat format);

VkPipelineVertexInputStateCreateInfo            createPipelineVertexInputStateCreateInfo(
    const VkVertexInputBindingDescription& bindingDescription,
    const std::vector<VkVertexInputAttributeDescription>& attributes);