#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "Resources.h"

struct GraphicPassShaderModules {
    VkShaderModule vertex;
    VkShaderModule fragment;
    VkShaderModule geometry;
    VkShaderModule tessEval;
    VkShaderModule tessCtrl;
};

VkShaderModule              createShaderModule(const std::vector<uint32_t>& spirV);
GraphicPassShaderModules    createGraphicPassShaderModules(const GraphicPassShaderSpirV& spirV);
void                        destroyGraphicPassShaderModules(const GraphicPassShaderModules& modules);
void                        destroyShaderModule(const VkShaderModule shaderModule);