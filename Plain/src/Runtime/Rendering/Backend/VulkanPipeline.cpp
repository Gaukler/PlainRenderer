#include "pch.h"
#include "VulkanPipeline.h"

VkPipelineInputAssemblyStateCreateInfo createInputAssemblyInfo(const RasterizationeMode rasterMode) {
    VkPipelineInputAssemblyStateCreateInfo info;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    info.primitiveRestartEnable = VK_FALSE;

    if (rasterMode == RasterizationeMode::Line) {
        info.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    }
    else if (rasterMode == RasterizationeMode::Point) {
        info.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }

    return info;
}

VkPipelineDynamicStateCreateInfo createDynamicStateInfo(const std::vector<VkDynamicState>& states) {
    VkPipelineDynamicStateCreateInfo dynamicStateInfo;
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.pNext = nullptr;
    dynamicStateInfo.flags = 0;
    dynamicStateInfo.dynamicStateCount = (uint32_t)states.size();
    dynamicStateInfo.pDynamicStates = states.data();
    return dynamicStateInfo;
}

VkPipelineRasterizationStateCreateInfo createRasterizationState(const RasterizationConfig& raster) {

    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    switch (raster.mode) {
    case RasterizationeMode::Fill:  polygonMode = VK_POLYGON_MODE_FILL; break;
    case RasterizationeMode::Line:  polygonMode = VK_POLYGON_MODE_LINE; break;
    case RasterizationeMode::Point:  polygonMode = VK_POLYGON_MODE_POINT; break;
    default: std::cout << "RenderBackend::createRasterizationState: Unknown RasterizationeMode\n"; break;
    };

    VkCullModeFlags cullFlags = VK_CULL_MODE_NONE;
    switch (raster.cullMode) {
    case CullMode::None:  cullFlags = VK_CULL_MODE_NONE; break;
    case CullMode::Front:  cullFlags = VK_CULL_MODE_FRONT_BIT; break;
    case CullMode::Back:  cullFlags = VK_CULL_MODE_BACK_BIT; break;
    default: std::cout << "RenderBackend::createRasterizationState unknown CullMode\n"; break;
    };

    VkPipelineRasterizationStateCreateInfo rasterInfo = {};

    rasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterInfo.pNext = nullptr;
    rasterInfo.flags = 0;
    rasterInfo.depthClampEnable = raster.clampDepth;
    rasterInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterInfo.polygonMode = polygonMode;
    rasterInfo.cullMode = cullFlags;
    rasterInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterInfo.depthBiasEnable = VK_FALSE;
    rasterInfo.depthBiasConstantFactor = 0.f;
    rasterInfo.depthBiasClamp = 0.f;
    rasterInfo.depthBiasSlopeFactor = 0.f;
    rasterInfo.lineWidth = 1.f;

    return rasterInfo;
}

VkPipelineRasterizationConservativeStateCreateInfoEXT createConservativeRasterCreateInfo() {
    VkPipelineRasterizationConservativeStateCreateInfoEXT state;
    state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT;
    state.pNext = nullptr;
    state.flags = 0;
    state.conservativeRasterizationMode = VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;
    return state;
}

VkPipelineViewportStateCreateInfo createDynamicViewportCreateInfo() {
    VkPipelineViewportStateCreateInfo viewportState;
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;
    viewportState.flags = 0;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr; // ignored as viewport is dynamic
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;  // ignored as viewport is dynamic
    return viewportState;
}