#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "Runtime/Rendering/ResourceDescriptions.h"

VkSampler createVulkanSampler(const SamplerDescription& desc);
VkFilter samplerInterpolationToVkFilter(const SamplerInterpolation mode);
VkSamplerAddressMode wrappingModeToVkSamplerAddressMode(const SamplerWrapping wrapping);
VkBorderColor samplerBorderColorToVkBorderColor(const SamplerBorderColor color);