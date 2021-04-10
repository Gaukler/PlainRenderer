#pragma once
#include "pch.h"
#include "vulkan/vulkan.h"
#include <spirv_cross_c.h>
#include "VertexInput.h"
#include "Runtime/Rendering/ResourceDescriptions.h"
#include "Resources.h"

/*
complete reflection data that is obtained from a set of SPIRV shaders
reflection is performed by SPIRV-Cross
set index is not a parameter as the reflection only takes care of the second set
second set is always pass specific, first is global, third is materials
*/
struct ShaderReflection {
    ShaderLayout        shaderLayout;
    VertexInputFlags    vertexInputFlags = VertexInputFlags(0);
    size_t              pushConstantByteSize = 0;
};

//error callback for the SPIRV-Cross library
void spirvCrossErrorCallback(void* userdata, const char* error);

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
ShaderReflection performShaderReflection(const GraphicPassShaderSpirV& shaderCode);
ShaderReflection performComputeShaderReflection(const std::vector<uint32_t>& shader);

void layoutFromSpirv(const std::vector<uint32_t>& spirv, const VkShaderStageFlags stageFlags, ShaderReflection* outReflection);