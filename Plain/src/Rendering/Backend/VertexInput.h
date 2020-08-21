#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

typedef enum VertexInputFlags {
    VERTEX_INPUT_POSITION_BIT = 0x00000001,
    VERTEX_INPUT_UV_BIT = 0x00000002,
    VERTEX_INPUT_NORMAL_BIT = 0x00000004,
    VERTEX_INPUT_TANGENT_BIT = 0x00000008,
    VERTEX_INPUT_BITANGENT_BIT = 0x00000010
} VertexInputFlags;

#define VERTEX_INPUT_ATTRIBUTE_COUNT 5

/*
defines which vertex attribute goes to which binding
*/
const VertexInputFlags vertexInputFlagPerLocation[VERTEX_INPUT_ATTRIBUTE_COUNT] = {
    VERTEX_INPUT_POSITION_BIT,
    VERTEX_INPUT_UV_BIT,
    VERTEX_INPUT_NORMAL_BIT,
    VERTEX_INPUT_TANGENT_BIT,
    VERTEX_INPUT_BITANGENT_BIT
};

/*
defines format per binding
*/
const VkFormat vertexInputFormatsPerLocation[VERTEX_INPUT_ATTRIBUTE_COUNT] = {
    VK_FORMAT_R32G32B32_SFLOAT, //position
    VK_FORMAT_R16G16_SFLOAT,    //uvs   
    VK_FORMAT_A2R10G10B10_SNORM_PACK32,  //normals
    VK_FORMAT_A2R10G10B10_SNORM_PACK32,  //tangent
    VK_FORMAT_A2R10G10B10_SNORM_PACK32   //bitanget
};

const uint32_t vertexInputBytePerLocation[VERTEX_INPUT_ATTRIBUTE_COUNT] = {
    12,
    4,
    4,
    4,
    4 
};