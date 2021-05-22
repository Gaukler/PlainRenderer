#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>
#include "Resources.h"
#include "VulkanSwapchain.h"

struct  GLFWwindow;
class   PerFrameResources;

struct ImGuiRenderResources {
    std::vector<VkRenderPassBeginInfo>  passBeginInfos;
    std::vector<VkFramebuffer>          framebuffers;
    VkRenderPass                        renderPass = VK_NULL_HANDLE;
    VkDescriptorPool                    descriptorPool = VK_NULL_HANDLE;
};

std::vector<VkFramebuffer>  createImGuiFramebuffers(const std::vector<Image>& targetImages, const VkRenderPass renderPass);
VkDescriptorPool            createImguiDescriptorPool();

std::vector<VkRenderPassBeginInfo> createImGuiPassBeginInfo(const int width, const int height,
    const std::vector<VkFramebuffer>& framebuffers, const VkRenderPass renderPass);

ImGuiRenderResources createImguiRenderResources(GLFWwindow* window, const Swapchain& swapchain, 
    const VkCommandBuffer cmdBuffer);

void recordImGuiRenderpass(PerFrameResources* inOutFrameResources, const std::vector<VkImageMemoryBarrier>& imageBarriers,
    const VkRenderPassBeginInfo& renderPassBeginInfo);

void imGuiShutdown(const ImGuiRenderResources& imguiResources);

void markImGuiNewFrame();