#include "pch.h"
#include "RenderPassManager.h"

RenderPassType getRenderPassType(const RenderPassHandle handle) {
    // first bit indicates pass type
    const bool firstBitSet = bool(handle.index >> 31);
    if (firstBitSet) {
        return RenderPassType::Graphic;
    }
    else {
        return RenderPassType::Compute;
    }
}

RenderPassHandle RenderPassManager::addGraphicPass(const GraphicPass pass) {
    uint32_t index = (uint32_t)m_graphicPasses.size();
    m_graphicPasses.push_back(pass);
    return indexToGraphicPassHandle(index);
}

RenderPassHandle RenderPassManager::addComputePass(const ComputePass pass) {
    uint32_t index = (uint32_t)m_computePasses.size();
    m_computePasses.push_back(pass);
    return indexToComputePassHandle(index);
}

uint32_t RenderPassManager::getGraphicPassCount() {
    return (uint32_t)m_graphicPasses.size();
}

uint32_t RenderPassManager::getComputePassCount() {
    return (uint32_t)m_computePasses.size();
}

GraphicPass& RenderPassManager::getGraphicPassRefByHandle(const RenderPassHandle handle) {
    assert(getRenderPassType(handle) == RenderPassType::Graphic);
    return m_graphicPasses[handleToIndex(handle)];
}

ComputePass& RenderPassManager::getComputePassRefByHandle(const RenderPassHandle handle) {
    assert(getRenderPassType(handle) == RenderPassType::Compute);
    return m_computePasses[handleToIndex(handle)];
}

GraphicPass& RenderPassManager::getGraphicPassRefByIndex(const uint32_t index) {
    return m_graphicPasses[index];
}

ComputePass& RenderPassManager::getComputePassRefByIndex(const uint32_t index) {
    return m_computePasses[index];
}

uint32_t RenderPassManager::handleToIndex(const RenderPassHandle handle) {
    //mask out first bit
    const uint32_t noUpperBitMask = 0x7FFFFFFF;
    return handle.index & noUpperBitMask;
}

RenderPassHandle RenderPassManager::indexToGraphicPassHandle(const uint32_t index) {
    //set first bit to 1 and cast
    const uint32_t upperBit = (uint32_t)1 << 31;
    return { index | upperBit };
}

RenderPassHandle RenderPassManager::indexToComputePassHandle(const uint32_t index) {
    //first bit should already be 0, just cast
    return { index };
}