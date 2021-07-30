#include "pch.h"
#include "VulkanDescriptorSet.h"
#include "VulkanContext.h"

VkDescriptorSet allocateVulkanDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool pool){
    VkDescriptorSetAllocateInfo setInfo;
    setInfo.sType               = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setInfo.pNext               = nullptr;
    setInfo.descriptorPool      = pool;
    setInfo.descriptorSetCount  = 1;
    setInfo.pSetLayouts         = &layout;

    VkDescriptorSet descriptorSet;
    auto result = vkAllocateDescriptorSets(vkContext.device, &setInfo, &descriptorSet);
    checkVulkanResult(result);

    return descriptorSet;
}