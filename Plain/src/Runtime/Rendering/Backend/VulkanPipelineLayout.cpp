#include "pch.h"
#include "VulkanPipelineLayout.h"
#include "VulkanContext.h"

VkPipelineLayoutCreateInfo createPipelineLayoutCreateInfo(const VkDescriptorSetLayout descriptorSetLayouts[3]) {

    const uint32_t setCount = 3;

    VkPipelineLayoutCreateInfo layoutInfo;
    layoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext                    = nullptr;
    layoutInfo.flags                    = 0;
    layoutInfo.setLayoutCount           = setCount;
    layoutInfo.pSetLayouts              = descriptorSetLayouts;
    layoutInfo.pushConstantRangeCount   = 0;
    layoutInfo.pPushConstantRanges      = nullptr;
    return layoutInfo;
}

VkPushConstantRange createPushConstantRange(const VkShaderStageFlags stageFlags, const size_t pushConstantSize) {
    VkPushConstantRange range;
    range.stageFlags    = stageFlags;
    range.offset        = 0;
    range.size          = (uint32_t)pushConstantSize;
    return range;
}

VkPipelineLayout createPipelineLayout(
    const VkDescriptorSetLayout descriptorSetLayouts[3],
    const size_t                pushConstantSize,
    const VkShaderStageFlags    stageFlags) {

    VkPipelineLayoutCreateInfo createInfo = createPipelineLayoutCreateInfo(descriptorSetLayouts);

    VkPushConstantRange pushConstantRange;
    const bool isUsingPushConstants = pushConstantSize > 0;
    if (isUsingPushConstants) {
        pushConstantRange                   = createPushConstantRange(stageFlags, pushConstantSize);
        createInfo.pPushConstantRanges      = &pushConstantRange;
        createInfo.pushConstantRangeCount   = 1;
    }

    VkPipelineLayout layout = {};
    const auto res = vkCreatePipelineLayout(vkContext.device, &createInfo, nullptr, &layout);
    checkVulkanResult(res);

    return layout;
}