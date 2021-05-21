#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

struct TimestampQueryPool {
    VkQueryPool vulkanHandle;
    uint32_t    queryCount = 0;
};

struct TimestampQuery {
    uint32_t startQuery = 0;
    uint32_t endQuery = 0;
    std::string name;
};

VkQueryPool createQueryPool(const VkQueryType queryType, const uint32_t queryCount);
uint32_t    issueTimestampQuery(const VkCommandBuffer cmdBuffer, TimestampQueryPool* inOutQueryPool);
void        resetTimestampQueryPool(TimestampQueryPool* inOutQueryPool);