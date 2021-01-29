#include "SpirvReflection.h"
#include "Utilities/GeneralUtils.h"

//callback not using userdata, resulting in a warning
//disable warning for the function
#pragma warning( push )
#pragma warning( disable : 4100 ) //C4100: unreferenced formal parameter

//---- private function declarations ----
size_t getSpirvCrossTypeByteSizeRecursive(const spvc_compiler& compiler, const spvc_type_id typeId);
void spirvCrossErrorCallback(void* userdata, const char* error);

//---- implementation ----

void spirvCrossErrorCallback(void* userdata, const char* error) {
    std::cout << error << std::endl;
}

//reenable warning
#pragma warning( pop )

ShaderReflection performShaderReflection(const GraphicPassShaderSpirV& shaderCode) {

    ShaderReflection reflection;
    
    layoutFromSpirv(shaderCode.vertex, VK_SHADER_STAGE_VERTEX_BIT, &reflection);
    layoutFromSpirv(shaderCode.fragment, VK_SHADER_STAGE_FRAGMENT_BIT, &reflection);

    if (shaderCode.geometry.has_value()) {
        layoutFromSpirv(shaderCode.geometry.value(), VK_SHADER_STAGE_GEOMETRY_BIT, &reflection);
    }
    if (shaderCode.tessellationControl.has_value()) {
        assert(shaderCode.tessellationEvaluation.has_value());
        layoutFromSpirv(shaderCode.tessellationControl.value(), VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, &reflection);
        layoutFromSpirv(shaderCode.tessellationEvaluation.value(), VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, &reflection);
    }

    return reflection;
}

ShaderReflection performComputeShaderReflection(const std::vector<uint32_t>& shader) {

    ShaderReflection reflection;
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings[4];

    layoutFromSpirv(shader, VK_SHADER_STAGE_COMPUTE_BIT, &reflection);

    return reflection;
}

void layoutFromSpirv(const std::vector<uint32_t>& spirv, const VkShaderStageFlags stageFlags, ShaderReflection* outReflection) {

    //pass shader to SPIRV cross 
    spvc_context                    spirvCrossContext   = NULL;
    spvc_parsed_ir                  ir                  = NULL;
    spvc_compiler                   compiler            = NULL;
    spvc_resources                  resources           = NULL;

    spvc_context_create(&spirvCrossContext);
    spvc_context_set_error_callback(spirvCrossContext, spirvCrossErrorCallback, nullptr);

    spvc_context_parse_spirv(spirvCrossContext, spirv.data(), spirv.size(), &ir);
    spvc_context_create_compiler(spirvCrossContext, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);
    spvc_compiler_create_shader_resources(compiler, &resources);

    /*
    layout bindings
    */

    //retrieve resource lists
    const spvc_reflected_resource*  samplerList         = NULL;
    const spvc_reflected_resource*  sampledImageList    = NULL;
    const spvc_reflected_resource*  storageImageList    = NULL;
    const spvc_reflected_resource*  storageBufferList   = NULL;
    const spvc_reflected_resource*  uniformBufferList   = NULL;
   
    size_t                          samplerCount        = NULL;
    size_t                          sampledImageCount   = NULL;
    size_t                          storageImageCount   = NULL;
    size_t                          storageBufferCount  = NULL;
    size_t                          uniformBufferCount  = NULL;

    spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS,  &samplerList,       &samplerCount);
    spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE,     &sampledImageList,  &sampledImageCount);
    spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE,      &storageImageList,  &storageImageCount);
    spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER,     &storageBufferList, &storageBufferCount);
    spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER,     &uniformBufferList, &uniformBufferCount);

    const size_t counts[5] = {
        samplerCount,
        sampledImageCount,
        storageImageCount,
        storageBufferCount,
        uniformBufferCount,
    };

    const spvc_reflected_resource* lists[5] = {
        samplerList,
        sampledImageList,
        storageImageList,
        storageBufferList,
        uniformBufferList,
    };

    std::vector<uint32_t>* results[5] = {
        &outReflection->shaderLayout.samplerBindings,
        &outReflection->shaderLayout.sampledImageBindings,
        &outReflection->shaderLayout.storageImageBindings,
        &outReflection->shaderLayout.storageBufferBindings,
        &outReflection->shaderLayout.uniformBufferBindings
    };

    //add if binding not yet in list
    for (size_t type = 0; type < 5; type++) {
        for (size_t i = 0; i < counts[type]; i++) {
            const auto id = lists[type][i].id;
            uint32_t set = spvc_compiler_get_decoration(compiler, id, SpvDecorationDescriptorSet);

            //set 0 is global, set 1 is per pass, set 2 is material
            //we only want bindings for set 1            
            if (set == 1) {
                const uint32_t binding = spvc_compiler_get_decoration(compiler, id, SpvDecorationBinding);
                if (!vectorContains(*results[type], binding)) {
                    results[type]->push_back(binding);
                }
            }
        }
    }

    //vertex input
    if (stageFlags & VK_SHADER_STAGE_VERTEX_BIT) {
        const spvc_reflected_resource* vertexInputList = NULL;
        size_t vertexInputCount = NULL;

        spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STAGE_INPUT, &vertexInputList, &vertexInputCount);
        //FIXME also lists unused variables
        for (size_t i = 0; i < vertexInputCount; i++) {
            const auto id = vertexInputList[i].id;

            const uint32_t location = spvc_compiler_get_decoration(compiler, id, SpvDecorationLocation);
            outReflection->vertexInputFlags = VertexInputFlags(outReflection->vertexInputFlags | vertexInputFlagPerLocation[location]);
        }
    }

	const spvc_reflected_resource* pushConstantList = nullptr;
	size_t pushConstantCount = 0;
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_PUSH_CONSTANT, &pushConstantList, &pushConstantCount);

	if (pushConstantCount > 1) {
		std::cout << "Warning: layoutFromSpirv(), push constant count > 1 is unexpected\n";
	}

	size_t pushConstantBlockSize = 0;
	for (int i = 0; i < pushConstantCount; i++) {
		const SpvId id = pushConstantList[i].id;
		pushConstantBlockSize += getSpirvCrossTypeByteSizeRecursive(compiler, pushConstantList[i].type_id);
	}
	//push constant block size can vary from stage to stage, e.g. fragment stage only needs first two members, but vertex has four
	//take maximum of all stages
	outReflection->pushConstantByteSize = glm::max(outReflection->pushConstantByteSize, pushConstantBlockSize);

    spvc_context_destroy(spirvCrossContext);
}

//TODO: more robust struct handling with stride and arrays
size_t getSpirvCrossTypeByteSizeRecursive(const spvc_compiler& compiler, const spvc_type_id typeId) {
	const spvc_type typeHandle = spvc_compiler_get_type_handle(compiler, typeId);
	const spvc_basetype baseType = spvc_type_get_basetype(typeHandle);

	size_t baseTypeSize = 0;

	if (baseType == spvc_basetype::SPVC_BASETYPE_BOOLEAN) {
		baseTypeSize = 4;
	}
	else if (
		baseType == spvc_basetype::SPVC_BASETYPE_INT8 ||
		baseType == spvc_basetype::SPVC_BASETYPE_UINT8) {
		baseTypeSize = 1;
	}
	else if (
		baseType == spvc_basetype::SPVC_BASETYPE_INT16 || 
		baseType == spvc_basetype::SPVC_BASETYPE_UINT16 || 
		baseType == spvc_basetype::SPVC_BASETYPE_FP16) {
		baseTypeSize = 2;
	}
	else if (
		baseType == spvc_basetype::SPVC_BASETYPE_INT32 ||
		baseType == spvc_basetype::SPVC_BASETYPE_UINT32 ||
		baseType == spvc_basetype::SPVC_BASETYPE_FP32) {
		baseTypeSize = 4;
	}
	else if (
		baseType == spvc_basetype::SPVC_BASETYPE_INT64 ||
		baseType == spvc_basetype::SPVC_BASETYPE_UINT64 ||
		baseType == spvc_basetype::SPVC_BASETYPE_FP64) {
		baseTypeSize = 8;
	}
	else if (baseType == spvc_basetype::SPVC_BASETYPE_STRUCT) {
		const int structMemberCount = spvc_type_get_num_member_types(typeHandle);
		for (int memberIndex = 0; memberIndex < structMemberCount; memberIndex++) {
			const spvc_type_id memberId = spvc_type_get_member_type(typeHandle, memberIndex);
			baseTypeSize += getSpirvCrossTypeByteSizeRecursive(compiler, memberId);
		}
	}
	else {
		std::cout << "Warning: getSpirvCrossTypeByteSizeRecursive(), encountered unknown type\n";
		return 0;
	}
	
	const size_t columns = spvc_type_get_columns(typeHandle);
	const size_t rows = spvc_type_get_vector_size(typeHandle);
	const size_t matrixEntryCount = columns * rows;

	return baseTypeSize * matrixEntryCount;
}