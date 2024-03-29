#pragma once
#include "pch.h"

const uint32_t invalidIndex = std::numeric_limits<uint32_t>::max();

// handles are contained in a struct to enforce type safety

struct MeshHandle {
    uint32_t index = invalidIndex;
};

struct RenderPassHandle {
    uint32_t index = invalidIndex;
};

enum class ImageHandleType : uint8_t { Default, Transient, Swapchain};

struct ImageHandle {
    ImageHandleType type    = ImageHandleType::Default;
    uint32_t        index   = invalidIndex;
};

struct SamplerHandle {
    uint32_t index = invalidIndex;
};

struct UniformBufferHandle {
    uint32_t index = invalidIndex;
};

struct StorageBufferHandle {
    uint32_t index = invalidIndex;
};

struct ComputeShaderHandle {
    uint32_t index = invalidIndex;
};

struct GraphicShadersHandle {
    uint32_t index = invalidIndex;
};