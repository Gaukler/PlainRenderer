#pragma once
#include "pch.h"
#include "RenderPass.h"

struct RenderPassExecutionEntry {
    RenderPassType type;
    int index;
};

RenderPassExecution getGenericRenderpassInfoFromExecutionEntry(
    const RenderPassExecutionEntry& entry,
    const std::vector<GraphicPassExecution>& graphicExecutions,
    const std::vector<ComputePassExecution>& computeExecutions);