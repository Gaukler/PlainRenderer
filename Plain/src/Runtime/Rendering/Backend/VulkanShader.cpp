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