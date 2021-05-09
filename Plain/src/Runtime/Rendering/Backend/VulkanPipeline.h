#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "Runtime/Rendering/ResourceDescriptions.h"

VkPipelineInputAssemblyStateCreateInfo createInputAssemblyInfo(const RasterizationeMode rasterMode);
VkPipelineDynamicStateCreateInfo createDynamicStateInfo(const std::vector<VkDynamicState>& states);
VkPipelineRasterizationStateCreateInfo createRasterizationState(const RasterizationConfig& raster);
VkPipelineRasterizationConservativeStateCreateInfoEXT createConservativeRasterCreateInfo();
VkPipelineViewportStateCreateInfo createDynamicViewportCreateInfo();