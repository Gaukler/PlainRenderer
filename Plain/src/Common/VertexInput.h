#pragma once
#include "pch.h"

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


//defines which vertex attribute goes to which binding
const VertexInputFlags vertexInputFlagPerLocation[VERTEX_INPUT_ATTRIBUTE_COUNT] = {
    VertexInputFlags::Position,
    VertexInputFlags::UV,
    VertexInputFlags::Normal,
    VertexInputFlags::Tangent,
    VertexInputFlags::Bitangent
};

const uint32_t vertexInputPositionByteSize  = 12;
const uint32_t vertexInputUVByteSize        = 4;
const uint32_t vertexInputNormalByteSize    = 4;
const uint32_t vertexInputTangentByteSize   = 4;
const uint32_t vertexInputBitangentByteSize = 4;

const uint32_t vertexInputBytePerLocation[VERTEX_INPUT_ATTRIBUTE_COUNT] = {
    vertexInputPositionByteSize,
    vertexInputUVByteSize,
    vertexInputNormalByteSize,
    vertexInputTangentByteSize,
    vertexInputBitangentByteSize
};

constexpr uint32_t getFullVertexFormatByteSize() {
    uint32_t size = 0;
    for (const uint32_t attributeSize : vertexInputBytePerLocation) {
        size += attributeSize;
    }
    return size;
}