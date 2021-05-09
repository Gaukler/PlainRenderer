#include "VulkanShaderModule.h"
#include "VulkanContext.h"

VkShaderModule createShaderModule(const std::vector<uint32_t>& spirV) {

    VkShaderModuleCreateInfo moduleInfo;
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.pNext = nullptr;
    moduleInfo.flags = 0;
    moduleInfo.codeSize = spirV.size() * sizeof(uint32_t);
    moduleInfo.pCode = spirV.data();

    VkShaderModule shaderModule;
    const auto res = vkCreateShaderModule(vkContext.device, &moduleInfo, nullptr, &shaderModule);
    checkVulkanResult(res);

    return shaderModule;
}

GraphicPassShaderModules createGraphicPassShaderModules(const GraphicPassShaderSpirV& spirV) {

    GraphicPassShaderModules modules;
    modules.vertex = createShaderModule(spirV.vertex);
    modules.fragment = createShaderModule(spirV.fragment);

    modules.geometry = VK_NULL_HANDLE;
    const bool hasGeometryShader = spirV.geometry.has_value();
    if (hasGeometryShader) {
        modules.geometry = createShaderModule(spirV.geometry.value());
    }

    modules.tessCtrl = VK_NULL_HANDLE;
    modules.tessEval = VK_NULL_HANDLE;
    const bool hasTesselationShaders =
        spirV.tessCtrl.has_value() &&
        spirV.tessEval.has_value();
    if (hasTesselationShaders) {
        modules.tessCtrl = createShaderModule(spirV.tessCtrl.value());
        modules.tessEval = createShaderModule(spirV.tessEval.value());
    }

    return modules;
}

void destroyGraphicPassShaderModules(const GraphicPassShaderModules& modules) {
    destroyShaderModule(modules.vertex);
    destroyShaderModule(modules.fragment);
    destroyShaderModule(modules.geometry);
    destroyShaderModule(modules.tessCtrl);
    destroyShaderModule(modules.tessEval);
}

void destroyShaderModule(const VkShaderModule shaderModule) {
    if (shaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(vkContext.device, shaderModule, nullptr);
    }
}