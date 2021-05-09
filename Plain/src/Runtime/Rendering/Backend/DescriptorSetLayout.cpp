#include "pch.h"
#include "DescriptorSetLayout.h"
#include "VulkanContext.h"

VkDescriptorSetLayout createDescriptorSetLayout(const ShaderLayout& shaderLayout) {

    const std::vector<uint32_t>* bindingLists[5] = {
        &shaderLayout.samplerBindings,
        &shaderLayout.sampledImageBindings,
        &shaderLayout.storageImageBindings,
        &shaderLayout.storageBufferBindings,
        &shaderLayout.uniformBufferBindings
    };

    const VkDescriptorType type[5] = {
        VK_DESCRIPTOR_TYPE_SAMPLER,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    };

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
    for (uint32_t typeIndex = 0; typeIndex < 5; typeIndex++) {
        for (const auto binding : *bindingLists[typeIndex]) {
            VkDescriptorSetLayoutBinding layoutBinding;
            layoutBinding.binding = binding;
            layoutBinding.descriptorType = type[typeIndex];
            layoutBinding.descriptorCount = 1;
            layoutBinding.stageFlags = VK_SHADER_STAGE_ALL;
            layoutBinding.pImmutableSamplers = nullptr;
            layoutBindings.push_back(layoutBinding);
        }
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.flags = 0;
    layoutInfo.bindingCount = (uint32_t)layoutBindings.size();
    layoutInfo.pBindings = layoutBindings.data();

    VkDescriptorSetLayout setLayout;
    auto res = vkCreateDescriptorSetLayout(vkContext.device, &layoutInfo, nullptr, &setLayout);
    checkVulkanResult(res);

    return setLayout;
}