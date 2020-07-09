#include "SpirvReflection.h"
#include "Utilities/GeneralUtils.h"

/*
=========
spirvCrossErrorCallback
=========
*/
void spirvCrossErrorCallback(void* userdata, const char* error) {
    std::cout << error << std::endl;
}

/*
=========
performShaderReflection
=========
*/
ShaderReflection performShaderReflection(const GraphicPassShaderSpirV& shaderCode) {

    ShaderReflection reflection;
    
    layoutFromSpirv(shaderCode.vertex, VK_SHADER_STAGE_VERTEX_BIT, &reflection);
    layoutFromSpirv(shaderCode.fragment, VK_SHADER_STAGE_FRAGMENT_BIT, &reflection);

    if (shaderCode.geometry.has_value()) {
        layoutFromSpirv(shaderCode.geometry.value(), VK_SHADER_STAGE_GEOMETRY_BIT, &reflection);
    }
    if (shaderCode.tesselationControl.has_value()) {
        assert(shaderCode.tesselationEvaluation.has_value());
        layoutFromSpirv(shaderCode.tesselationControl.value(), VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, &reflection);
        layoutFromSpirv(shaderCode.tesselationEvaluation.value(), VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, &reflection);
    }

    return reflection;
}

/*
=========
performComputeShaderReflection
=========
*/
ShaderReflection performComputeShaderReflection(const std::vector<uint32_t>& shader) {

    ShaderReflection reflection;
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings[4];

    layoutFromSpirv(shader, VK_SHADER_STAGE_COMPUTE_BIT, &reflection);

    return reflection;
}

/*
=========
VkDescriptorSetLayoutBindingsFromSpirv
=========
*/
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

            /*
            set 0 is global, set 1 is per pass, set 2 is material
            we only want bindings for set 1
            */
            if (set == 1) {
                const uint32_t binding = spvc_compiler_get_decoration(compiler, id, SpvDecorationBinding);
                if (!vectorContains(*results[type], binding)) {
                    results[type]->push_back(binding);
                }
            }
            /*
            material features
            */
            if (set == 2) {
                const uint32_t binding = spvc_compiler_get_decoration(compiler, id, SpvDecorationBinding);
                if (binding == 0 || binding == 4) {
                    outReflection->materialFeatures = (MaterialFeatureFlags)(outReflection->materialFeatures | MATERIAL_FEATURE_FLAG_ALBEDO_TEXTURE);
                }
                if (binding == 1 || binding == 5) {
                    outReflection->materialFeatures = (MaterialFeatureFlags)(outReflection->materialFeatures | MATERIAL_FEATURE_FLAG_NORMAL_TEXTURE);
                }
                if (binding == 2 || binding == 6) {
                    outReflection->materialFeatures = (MaterialFeatureFlags)(outReflection->materialFeatures | MATERIAL_FEATURE_FLAG_METALIC_TEXTURE);
                }
                if (binding == 3 || binding == 7) {
                    outReflection->materialFeatures = (MaterialFeatureFlags)(outReflection->materialFeatures | MATERIAL_FEATURE_FLAG_ROUGHNESS_TEXTURE);
                }
            }
        }
    }

    /*
    vertex input
    */
    if (stageFlags & VK_SHADER_STAGE_VERTEX_BIT) {
        const spvc_reflected_resource*  vertexInputList     = NULL;
        size_t                          vertexInputCount    = NULL;

        spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STAGE_INPUT, &vertexInputList, &vertexInputCount);
        //FIXME also lists unused variables
        for (size_t i = 0; i < vertexInputCount; i++) {
            const auto id = vertexInputList[i].id;

            const uint32_t location = spvc_compiler_get_decoration(compiler, id, SpvDecorationLocation);
            outReflection->vertexInputFlags = VertexInputFlags(outReflection->vertexInputFlags | vertexInputFlagPerLocation[location]);
        }
    }

    /*
    cleanup
    */
    spvc_context_destroy(spirvCrossContext);
}