#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "VulkanTimestampQueries.h"

struct PerFrameResources {
    std::vector<VkFramebuffer>  transientFramebuffers;
    VkCommandBuffer             commandBuffer;
    TimestampQueryPool          timestampQueryPool;
    std::vector<TimestampQuery> timestampQueries;
};

PerFrameResources   createPerFrameResources(const VkCommandPool cmdPool);
void                destroyPerFrameResources(PerFrameResources* inOutFrameResources);