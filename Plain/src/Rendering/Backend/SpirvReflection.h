#pragma once
#include "pch.h"
#include "vulkan/vulkan.h"
#include <spirv_cross_c.h>
#include "VertexInput.h"
#include "Rendering/ResourceDescriptions.h"
#include "Resources.h"

/*
complete reflection data that is obtained from a set of SPIRV shaders
reflection is performed by SPIRV-Cross
set index is not a parameter as the reflection only takes care of the second set
second set is always pass specific, first is global, third is materials
*/
struct ShaderReflection {
    ShaderLayout            shaderLayout;
    VertexInputFlags        vertexInputFlags = VertexInputFlags(0);
    MaterialFeatureFlags    materialFeatures = MaterialFeatureFlags(0);
};

/*
error callback for the SPIRV-Cross library
*/
void spirvCrossErrorCallback(void* userdata, const char* error);

//input for shader reflection
struct GraphicShaderCode {
    std::vector<char> vertexCode;
    std::vector<char> fragmentCode;
    std::optional<std::vector<char>> geometryCode;
    std::optional<std::vector<char>> tesselationControlCode;
    std::optional<std::vector<char>> tesselationEvaluationCode;
};

/*
=======
descriptor set layout functions
=======
*/

/*
primary functions to retrieve layout from SPIRV
only the second set is reflected
the first and third are global and material layouts, both of which are predefined
*/
ShaderReflection performShaderReflection(const GraphicShaderCode& shaderCode);
ShaderReflection performComputeShaderReflection(const std::vector<char>& shader);

void layoutFromSpirv(const std::vector<char>& spirv, const VkShaderStageFlags stageFlags, ShaderReflection* outReflection);