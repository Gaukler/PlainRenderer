#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "Resources.h"

VkShaderModule createShaderModule(const std::vector<uint32_t>& spirV);

struct GraphicPassShaderModules {
    VkShaderModule vertex;
    VkShaderModule fragment;
    VkShaderModule geometry;
    VkShaderModule tessEval;
    VkShaderModule tessCtrl;
};

GraphicPassShaderModules createGraphicPassShaderModules(const GraphicPassShaderSpirV& spirV);
void destroyGraphicPassShaderModules(const GraphicPassShaderModules& modules);
void destroyShaderModule(const VkShaderModule shaderModule);