#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

VkPipelineLayoutCreateInfo  createPipelineLayoutCreateInfo(const VkDescriptorSetLayout descriptorSetLayouts[3]);
VkPushConstantRange         createPushConstantRange(const VkShaderStageFlags stageFlags, const size_t pushConstantSize);

VkPipelineLayout            createPipelineLayout(
    const VkDescriptorSetLayout descriptorSetLayouts[3],
    const size_t                pushConstantSize,
    const VkShaderStageFlags    stageFlags);