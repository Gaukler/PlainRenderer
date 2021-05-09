#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "Resources.h"

VkDescriptorSetLayout createDescriptorSetLayout(const ShaderLayout& shaderLayout);