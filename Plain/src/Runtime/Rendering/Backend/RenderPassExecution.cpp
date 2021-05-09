#include "pch.h"
#include "RenderPassExecution.h"

RenderPassExecution getGenericRenderpassInfoFromExecutionEntry(
    const RenderPassExecutionEntry& entry,
    const std::vector<GraphicPassExecution>& graphicExecutions,
    const std::vector<ComputePassExecution>& computeExecutions) {

    if (entry.type == RenderPassType::Graphic) {
        return graphicExecutions[entry.index].genericInfo;
    }
    else if (entry.type == RenderPassType::Compute) {
        return computeExecutions[entry.index].genericInfo;
    }
    else {
        std::cout << "Unknown RenderPassType\n";
        throw("Unknown RenderPassType");
    }
}