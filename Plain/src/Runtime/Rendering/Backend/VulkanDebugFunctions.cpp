#include "pch.h"
#include "VulkanDebug.h"
#include "VulkanContext.h"

VulkanDebugUtilsFunctions gDebugFunctionPtrs;

void initVulkanDebugFunctions() {
    gDebugFunctionPtrs.vkCmdBeginDebugUtilsLabelEXT     = (PFN_vkCmdBeginDebugUtilsLabelEXT)    vkGetDeviceProcAddr(vkContext.device, "vkCmdBeginDebugUtilsLabelEXT");
    gDebugFunctionPtrs.vkCmdEndDebugUtilsLabelEXT       = (PFN_vkCmdEndDebugUtilsLabelEXT)      vkGetDeviceProcAddr(vkContext.device, "vkCmdEndDebugUtilsLabelEXT");
    gDebugFunctionPtrs.vkCmdInsertDebugUtilsLabelEXT    = (PFN_vkCmdInsertDebugUtilsLabelEXT)   vkGetDeviceProcAddr(vkContext.device, "vkCmdInsertDebugUtilsLabelEXT");
    gDebugFunctionPtrs.vkCreateDebugUtilsMessengerEXT   = (PFN_vkCreateDebugUtilsMessengerEXT)  vkGetDeviceProcAddr(vkContext.device, "vkCreateDebugUtilsMessengerEXT");
    gDebugFunctionPtrs.vkDestroyDebugUtilsMessengerEXT  = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetDeviceProcAddr(vkContext.device, "vkDestroyDebugUtilsMessengerEXT");
    gDebugFunctionPtrs.vkQueueBeginDebugUtilsLabelEXT   = (PFN_vkQueueBeginDebugUtilsLabelEXT)  vkGetDeviceProcAddr(vkContext.device, "vkQueueBeginDebugUtilsLabelEXT");
    gDebugFunctionPtrs.vkQueueEndDebugUtilsLabelEXT     = (PFN_vkQueueEndDebugUtilsLabelEXT)    vkGetDeviceProcAddr(vkContext.device, "vkQueueEndDebugUtilsLabelEXT");
    gDebugFunctionPtrs.vkQueueInsertDebugUtilsLabelEXT  = (PFN_vkQueueInsertDebugUtilsLabelEXT) vkGetDeviceProcAddr(vkContext.device, "vkQueueInsertDebugUtilsLabelEXT");
    gDebugFunctionPtrs.vkSetDebugUtilsObjectNameEXT     = (PFN_vkSetDebugUtilsObjectNameEXT)    vkGetDeviceProcAddr(vkContext.device, "vkSetDebugUtilsObjectNameEXT");
    gDebugFunctionPtrs.vkSetDebugUtilsObjectTagEXT      = (PFN_vkSetDebugUtilsObjectTagEXT)     vkGetDeviceProcAddr(vkContext.device, "vkSetDebugUtilsObjectTagEXT");
    gDebugFunctionPtrs.vkSubmitDebugUtilsMessageEXT     = (PFN_vkSubmitDebugUtilsMessageEXT)    vkGetDeviceProcAddr(vkContext.device, "vkSubmitDebugUtilsMessageEXT");
}

void startDebugLabel(const VkCommandBuffer cmdBuffer, const std::string& name) {
    VkDebugUtilsLabelEXT label;
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pNext = nullptr;
    label.pLabelName = name.c_str();
    label.color[0] = 1.f;
    label.color[1] = 1.f;
    label.color[2] = 1.f;
    label.color[3] = 1.f;
    gDebugFunctionPtrs.vkCmdBeginDebugUtilsLabelEXT(cmdBuffer, &label);
}

void endDebugLabel(const VkCommandBuffer cmdBuffer) {
    gDebugFunctionPtrs.vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
}