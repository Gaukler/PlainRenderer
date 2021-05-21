#include "pch.h"
#include "VulkanTimestampQueries.h"
#include "VulkanContext.h"

VkQueryPool createQueryPool(const VkQueryType queryType, const uint32_t queryCount) {

    VkQueryPoolCreateInfo createInfo;
    createInfo.sType                = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    createInfo.pNext                = nullptr;
    createInfo.flags                = 0;
    createInfo.queryType            = queryType;
    createInfo.queryCount           = queryCount;
    createInfo.pipelineStatistics   = 0; // pipeline queries not handled for now

    VkQueryPool pool;
    const auto result = vkCreateQueryPool(vkContext.device, &createInfo, nullptr, &pool);
    checkVulkanResult(result);

    vkResetQueryPool(vkContext.device, pool, 0, queryCount);

    return pool;
}

uint32_t issueTimestampQuery(const VkCommandBuffer cmdBuffer, TimestampQueryPool *inOutQueryPool) {
    assert(inOutQueryPool);
    const uint32_t query = inOutQueryPool->queryCount;
    vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, inOutQueryPool->vulkanHandle, query);
    inOutQueryPool->queryCount++;
    return query;
}

void resetTimestampQueryPool(TimestampQueryPool *inOutQueryPool) {
    assert(inOutQueryPool);
    vkResetQueryPool(vkContext.device, inOutQueryPool->vulkanHandle, 0, inOutQueryPool->queryCount);
    inOutQueryPool->queryCount = 0;
}