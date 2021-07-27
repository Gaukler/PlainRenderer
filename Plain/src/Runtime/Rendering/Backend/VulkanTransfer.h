#pragma once
#include "pch.h"
#include "vulkan/vulkan.h"
#include "Resources.h"

// FIXME: "Data" is a pretty common name, solve by using namespace
struct Data {
    const void*  ptr;
    const size_t size;

    Data(const void* ptr, const size_t size);
    Data();
};

struct TransferResources {
    Buffer          stagingBuffer;
    VkCommandPool   transientCmdPool;
};