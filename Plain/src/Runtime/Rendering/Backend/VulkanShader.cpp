#include "pch.h"
#include "VulkanShader.h"

std::vector<VkSpecializationMapEntry> createShaderSpecialisationMapEntries(
    const std::vector<SpecialisationConstant>& constants) {

    std::vector<VkSpecializationMapEntry> specialisationMap;

    uint32_t currentOffset = 0;
    for (const auto c : constants) {
        const uint32_t constantSize = (uint32_t)c.data.size();

        VkSpecializationMapEntry entry;
        entry.constantID = c.location;
        entry.offset = currentOffset;
        entry.size = constantSize;
        specialisationMap.push_back(entry);

        currentOffset += constantSize;
    }

    return specialisationMap;
}

size_t computeSpecialisationConstantTotalDataSize(const std::vector<VkSpecializationMapEntry>& constants) {
    if (constants.size() == 0) {
        return 0;
    }
    else {
        const auto& lastConstant = constants.back();
        return lastConstant.offset + lastConstant.size;
    }
}

std::vector<char> copySpecialisationDataIntoSingleArray(const size_t totalDataSize,
    const std::vector<SpecialisationConstant>& constants) {

    std::vector<char> dataArray(totalDataSize);
    size_t currentOffset = 0;
    for (const auto& constant : constants) {
        memcpy(dataArray.data() + currentOffset, constant.data.data(), constant.data.size());
        currentOffset += constant.data.size();
    }
    return dataArray;
}

VkSpecializationInfo createShaderSpecialisationInfo(const std::vector<char>& specialisationData, 
    const std::vector<VkSpecializationMapEntry>& specialisationMap) {

    VkSpecializationInfo info;
    info.dataSize = specialisationData.size() * sizeof(char);
    info.pData = specialisationData.data();
    info.mapEntryCount = specialisationMap.size();
    info.pMapEntries = specialisationMap.data();
    return info;
}

void createShaderSpecialisationStructs(const std::vector<SpecialisationConstant>& constants,
    ShaderSpecialisationStructs* outStructs){
    assert(outStructs);
    outStructs->mapEntries = createShaderSpecialisationMapEntries(constants);
    const size_t totalDataSize = computeSpecialisationConstantTotalDataSize(outStructs->mapEntries);
    outStructs->data = copySpecialisationDataIntoSingleArray(totalDataSize, constants);
    outStructs->info = createShaderSpecialisationInfo(outStructs->data, outStructs->mapEntries);
}

std::vector<VkPipelineShaderStageCreateInfo> createGraphicPipelineShaderCreateInfo(
    const GraphicPassShaderDescriptions& shaders, 
    const GraphicPassShaderModules& shaderModules, 
    GraphicPassSpecialisationStructs* outSpecialisationStructs) {

    std::vector<VkPipelineShaderStageCreateInfo> stages;

    createShaderSpecialisationStructs(shaders.vertex.specialisationConstants,
        &outSpecialisationStructs->vertex);
    stages.push_back(createPipelineShaderStageInfos(shaderModules.vertex, VK_SHADER_STAGE_VERTEX_BIT,
        &outSpecialisationStructs->vertex.info));

    createShaderSpecialisationStructs(shaders.fragment.specialisationConstants, 
        &outSpecialisationStructs->fragment);
    stages.push_back(createPipelineShaderStageInfos(shaderModules.fragment, VK_SHADER_STAGE_FRAGMENT_BIT,
        &outSpecialisationStructs->fragment.info));

    if (graphicPassModulesHaveGeometryShader(shaderModules)) {
        createShaderSpecialisationStructs(shaders.geometry.value().specialisationConstants,
            &outSpecialisationStructs->geometry);
        stages.push_back(createPipelineShaderStageInfos(shaderModules.geometry, VK_SHADER_STAGE_GEOMETRY_BIT,
            &outSpecialisationStructs->geometry.info));
    }
    if (graphicPassModulesHaveTesselationShaders(shaderModules)) {

        createShaderSpecialisationStructs(
            shaders.tessCtrl.value().specialisationConstants, &outSpecialisationStructs->tessCtrl);
        createShaderSpecialisationStructs(
            shaders.tessEval.value().specialisationConstants, &outSpecialisationStructs->tessEval);

        stages.push_back(createPipelineShaderStageInfos(shaderModules.tessCtrl, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
            &outSpecialisationStructs->tessCtrl.info));
        stages.push_back(createPipelineShaderStageInfos(shaderModules.tessEval, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
            &outSpecialisationStructs->tessEval.info));
    }
    return stages;
}

VkPipelineShaderStageCreateInfo createPipelineShaderStageInfos(
    const VkShaderModule module,
    const VkShaderStageFlagBits stage,
    const VkSpecializationInfo* pSpecialisationInfo) {

    VkPipelineShaderStageCreateInfo createInfos;
    createInfos.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfos.pNext = nullptr;
    createInfos.flags = 0;
    createInfos.stage = stage;
    createInfos.module = module;
    createInfos.pName = "main";
    createInfos.pSpecializationInfo = pSpecialisationInfo;

    return createInfos;
}

VkShaderStageFlags getGraphicPassShaderStageFlags(const GraphicPassShaderModules shaderModules) {
    VkShaderStageFlags pipelineLayoutStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    if (graphicPassModulesHaveGeometryShader(shaderModules)) {
        pipelineLayoutStageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    }
    if (graphicPassModulesHaveTesselationShaders(shaderModules)) {
        pipelineLayoutStageFlags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        pipelineLayoutStageFlags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    }
    return pipelineLayoutStageFlags;
}

bool graphicPassModulesHaveGeometryShader(const GraphicPassShaderModules shaderModules) {
    return shaderModules.geometry != VK_NULL_HANDLE;
}

bool graphicPassModulesHaveTesselationShaders(const GraphicPassShaderModules shaderModules) {
    return shaderModules.tessEval != VK_NULL_HANDLE && shaderModules.tessCtrl != VK_NULL_HANDLE;
}