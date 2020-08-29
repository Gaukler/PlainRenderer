#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

enum class VertexInputFlags {
    Position    = 0x00000001,
    UV          = 0x00000002,
    Normal      = 0x00000004,
    Tangent     = 0x00000008,
    Bitangent   = 0x00000010
};

VertexInputFlags operator&(const VertexInputFlags l, const VertexInputFlags r);
VertexInputFlags operator|(const VertexInputFlags l, const VertexInputFlags r);

#define VERTEX_INPUT_ATTRIBUTE_COUNT 5

/*
defines which vertex attribute goes to which binding
*/
const VertexInputFlags vertexInputFlagPerLocation[VERTEX_INPUT_ATTRIBUTE_COUNT] = {
    VertexInputFlags::Position,
    VertexInputFlags::UV,
    VertexInputFlags::Normal,
    VertexInputFlags::Tangent,
    VertexInputFlags::Bitangent
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