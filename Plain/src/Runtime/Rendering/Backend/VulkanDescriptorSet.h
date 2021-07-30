#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

VkDescriptorSet allocateVulkanDescriptorSet(VkDescriptorSetLayout layout, VkDescriptorPool pool);