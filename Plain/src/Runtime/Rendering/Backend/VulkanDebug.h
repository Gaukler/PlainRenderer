#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

// because they are extensions they need to be manually acquired using vkGetDeviceProcAddr
struct VulkanDebugUtilsFunctions {
    PFN_vkCmdBeginDebugUtilsLabelEXT    vkCmdBeginDebugUtilsLabelEXT    = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT      vkCmdEndDebugUtilsLabelEXT      = nullptr;
    PFN_vkCmdInsertDebugUtilsLabelEXT   vkCmdInsertDebugUtilsLabelEXT   = nullptr;
    PFN_vkCreateDebugUtilsMessengerEXT  vkCreateDebugUtilsMessengerEXT  = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;
    PFN_vkQueueBeginDebugUtilsLabelEXT  vkQueueBeginDebugUtilsLabelEXT  = nullptr;
    PFN_vkQueueEndDebugUtilsLabelEXT    vkQueueEndDebugUtilsLabelEXT    = nullptr;
    PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT = nullptr;
    PFN_vkSetDebugUtilsObjectNameEXT    vkSetDebugUtilsObjectNameEXT    = nullptr;
    PFN_vkSetDebugUtilsObjectTagEXT     vkSetDebugUtilsObjectTagEXT     = nullptr;
    PFN_vkSubmitDebugUtilsMessageEXT    vkSubmitDebugUtilsMessageEXT    = nullptr;
};

void initVulkanDebugFunctions();
void startDebugLabel(const VkCommandBuffer cmdBuffer, const std::string& name);
void endDebugLabel(const VkCommandBuffer cmdBuffer);