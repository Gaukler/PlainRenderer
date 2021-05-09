#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "Resources.h"

std::vector<VkSpecializationMapEntry> createShaderSpecialisationMapEntries(
    const std::vector<SpecialisationConstant>& constants);

size_t computeSpecialisationConstantTotalDataSize(const std::vector<VkSpecializationMapEntry>& constants);
std::vector<char> copySpecialisationDataIntoSingleArray(const size_t totalDataSize, 
    const std::vector<SpecialisationConstant>& constants);

VkSpecializationInfo createShaderSpecialisationInfo(const std::vector<char>& specialisationData,
    const std::vector<VkSpecializationMapEntry>& specialisationMap);

struct ShaderSpecialisationStructs {
    VkSpecializationInfo info;
    std::vector<VkSpecializationMapEntry> mapEntries;
    std::vector<char> data;
};

void createShaderSpecialisationStructs(const std::vector<SpecialisationConstant>& constants,
    ShaderSpecialisationStructs* outStructs);