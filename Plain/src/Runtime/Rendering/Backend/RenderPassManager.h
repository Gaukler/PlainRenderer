#pragma once
#include "pch.h"
#include "Resources.h"

enum class RenderPassType { Graphic, Compute };

RenderPassType getRenderPassType(const RenderPassHandle handle);

/*
RenderPass handles are shared between compute and graphics to allow easy dependency management in the frontend
the first bit of the handle indicates wether a handle is a compute or graphic pass
this class wraps the vectors and indexing to avoid having to explicitly call a translation function for every access
e.g. passes(handleToIndex(handle))
*/
class RenderPassManager {
public:

    static RenderPassManager& getRef();
    RenderPassManager(const RenderPassManager&) = delete;

    RenderPassHandle addGraphicPass(const GraphicPass pass);
    RenderPassHandle addComputePass(const ComputePass pass);

    uint32_t getGraphicPassCount();
    uint32_t getComputePassCount();

    GraphicPass& getGraphicPassRefByHandle(const RenderPassHandle handle);
    ComputePass& getComputePassRefByHandle(const RenderPassHandle handle);

    GraphicPass& getGraphicPassRefByIndex(const uint32_t index);
    ComputePass& getComputePassRefByIndex(const uint32_t index);

private:
    RenderPassManager() {};
    std::vector<GraphicPass> m_graphicPasses;
    std::vector<ComputePass> m_computePasses;

    uint32_t handleToIndex(const RenderPassHandle handle);
    RenderPassHandle indexToGraphicPassHandle(const uint32_t index);
    RenderPassHandle indexToComputePassHandle(const uint32_t index);
};