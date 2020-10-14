#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "VertexInput.h"

//defines format per binding
const VkFormat vertexInputFormatsPerLocation[VERTEX_INPUT_ATTRIBUTE_COUNT] = {
    VK_FORMAT_R32G32B32_SFLOAT, //position
    VK_FORMAT_R16G16_SFLOAT,    //uvs   
    VK_FORMAT_A2R10G10B10_SNORM_PACK32,  //normals
    VK_FORMAT_A2R10G10B10_SNORM_PACK32,  //tangent
    VK_FORMAT_A2R10G10B10_SNORM_PACK32   //bitanget
};