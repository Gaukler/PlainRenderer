#include "pch.h"
#include "VulkanTimestampQueries.h"
#include "VulkanContext.h"

float g_nanosecondsPerTimestamp = 0.f;

void initVulkanTimestamps() {
    VkPhysicalDeviceProperties deviceProperties = getVulkanDeviceProperties();
    g_nanosecondsPerTimestamp = deviceProperties.limits.timestampPeriod;
}

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

std::vector<RenderPassTime> retrieveRenderPassTimes(
    const TimestampQueryPool&           queryPool,
    const std::vector<TimestampQuery>&  timestampQueries) {

    std::vector<uint32_t>   timestamps(queryPool.queryCount);

    // res = vkGetQueryPoolResults(vkContext.device, m_timestampQueryPool, 0, m_currentTimestampQueryCount,
    //    timestamps.size() * sizeof(uint32_t), timestamps.data(), 0, VK_QUERY_RESULT_WAIT_BIT);
    // assert(res == VK_SUCCESS);
    // on Ryzen 4700U iGPU vkGetQueryPoolResults only returns correct results for the first query
    // maybe it contains more info so needs more space per query?
    // manually get every query for now
    // FIXME: proper solution
    for (size_t i = 0; i < queryPool.queryCount; i++) {
        auto result = vkGetQueryPoolResults(
            vkContext.device,
            queryPool.vulkanHandle,
            (uint32_t)i,
            1,
            (uint32_t)timestamps.size() * sizeof(uint32_t),
            &timestamps[i],
            0,
            VK_QUERY_RESULT_WAIT_BIT);
        checkVulkanResult(result);
    }

    std::vector<RenderPassTime> times;
    times.reserve(timestampQueries.size());

    for (const TimestampQuery query : timestampQueries) {

        const uint32_t startTime = timestamps[query.startQuery];
        const uint32_t endTime = timestamps[query.endQuery];
        const uint32_t time = endTime - startTime;

        const float nanoseconds = (float)time * g_nanosecondsPerTimestamp;
        const float milliseconds = nanoseconds * 0.000001f;

        RenderPassTime timing;
        timing.name = query.name;
        timing.timeMs = milliseconds;
        times.push_back(timing);
    }

    return times;
}