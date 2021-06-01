#include "pch.h"
#include "ImGuiIntegration.h"
#include <imgui/imgui.h>
#include "VulkanFramebuffer.h"
#include "VulkanContext.h"
#include "VulkanRenderPass.h"
#include "RenderPass.h"
#include "VulkanImageFormats.h"
#include "VulkanCommandRecording.h"
#include "PerFrameResources.h"
#include "VulkanTimestampQueries.h"

// disable ImGui warnings
#pragma warning( push )
#pragma warning( disable : 26495 26812)

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>

// reenable warnings
#pragma warning( pop )

std::vector<VkFramebuffer> createImGuiFramebuffers(const std::vector<Image>& targetImages, const VkRenderPass renderPass) {
    std::vector<VkFramebuffer> buffers;
    for (const auto& uiTarget : targetImages) {
        assert(uiTarget.viewPerMip.size() > 0);
        const VkImageView   targetView  = uiTarget.viewPerMip.front();
        const uint32_t      &width      = uiTarget.desc.width;
        const uint32_t      &height     = uiTarget.desc.height;
        const VkFramebuffer framebuffer = createVulkanFramebuffer({ targetView }, renderPass, width, height);
        buffers.push_back(framebuffer);
    }
    return buffers;
}

VkDescriptorPool createImguiDescriptorPool() {
    // taken from imgui vulkan example, could not find any info if imgui can work with less allocations
    VkDescriptorPoolSize poolSizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets       = 1000 * IM_ARRAYSIZE(poolSizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
    pool_info.pPoolSizes    = poolSizes;

    VkDescriptorPool pool;
    const auto result = vkCreateDescriptorPool(vkContext.device, &pool_info, nullptr, &pool);
    checkVulkanResult(result);

    return pool;
}

std::vector<VkRenderPassBeginInfo> createImGuiPassBeginInfo(const int width, const int height,
    const std::vector<VkFramebuffer>& framebuffers, const VkRenderPass renderPass) {
    std::vector<VkRenderPassBeginInfo> passBeginInfos;
    for (const auto& framebuffer : framebuffers) {
        const auto beginInfo = createRenderPassBeginInfo(width, height, renderPass, framebuffer, {});
        passBeginInfos.push_back(beginInfo);
    }
    return passBeginInfos;
}

ImGuiRenderResources createImguiRenderResources(GLFWwindow* window, const Swapchain& swapchain, const VkCommandBuffer cmdBuffer) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    const ImageFormat   swapchainFormat = vulkanImageFormatToImageFormat(swapchain.surfaceFormat.format);
    const Attachment    colorAttachment = Attachment(swapchainFormat, AttachmentLoadOp::Load);

    ImGuiRenderResources imguiResources;
    imguiResources.renderPass = createVulkanRenderPass(std::vector<Attachment> {colorAttachment});
    imguiResources.descriptorPool = createImguiDescriptorPool();

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = vkContext.vulkanInstance;
    init_info.PhysicalDevice = vkContext.physicalDevice;
    init_info.Device = vkContext.device;
    init_info.QueueFamily = vkContext.queueFamilies.graphics;
    init_info.Queue = vkContext.graphicQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imguiResources.descriptorPool;
    init_info.Allocator = nullptr;
    init_info.MinImageCount = swapchain.minImageCount;
    init_info.ImageCount = (uint32_t)swapchain.images.size();
    init_info.CheckVkResultFn = nullptr;
    if (!ImGui_ImplVulkan_Init(&init_info, imguiResources.renderPass)) {
        throw("ImGui inizialisation error");
    }

    // build fonts texture
    beginCommandBuffer(cmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer);

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &cmdBuffer;

    endCommandBufferRecording(cmdBuffer);
    auto res = vkQueueSubmit(vkContext.graphicQueue, 1, &end_info, VK_NULL_HANDLE);
    assert(res == VK_SUCCESS);

    waitForGpuIdle();
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    imguiResources.framebuffers = createImGuiFramebuffers(swapchain.images, imguiResources.renderPass);

    const int width = swapchain.images.front().desc.width;
    const int height = swapchain.images.front().desc.height;
    imguiResources.passBeginInfos = createImGuiPassBeginInfo(width, height, imguiResources.framebuffers, imguiResources.renderPass);

    return imguiResources;
}

void recordImGuiRenderpass(PerFrameResources* inOutFrameResources, const std::vector<VkImageMemoryBarrier> &imageBarriers,
    const VkRenderPassBeginInfo &renderPassBeginInfo) {
    assert(inOutFrameResources);

    TimestampQuery imguiQuery;
    imguiQuery.name = "ImGui";
    imguiQuery.startQuery = issueTimestampQuery(inOutFrameResources->commandBuffer, &inOutFrameResources->timestampQueryPool);

    ImGui::Render();

    vkCmdPipelineBarrier(inOutFrameResources->commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1, imageBarriers.data());

    vkCmdBeginRenderPass(inOutFrameResources->commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), inOutFrameResources->commandBuffer);
    vkCmdEndRenderPass(inOutFrameResources->commandBuffer);

    imguiQuery.endQuery = issueTimestampQuery(inOutFrameResources->commandBuffer, &inOutFrameResources->timestampQueryPool);
    inOutFrameResources->timestampQueries.push_back(imguiQuery);
}

void imGuiShutdown(const ImGuiRenderResources &imguiResources) {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyRenderPass(vkContext.device, imguiResources.renderPass, nullptr);
    destroyFramebuffers(imguiResources.framebuffers);
    vkDestroyDescriptorPool(vkContext.device, imguiResources.descriptorPool, nullptr);
}

void markImGuiNewFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}