#pragma once
#include <vulkan/vulkan.h>
#include "../ResourceDescriptions.h"

VkSampler createVulkanSampler(const SamplerDescription& desc);