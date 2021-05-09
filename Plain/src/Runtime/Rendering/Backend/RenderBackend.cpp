#include "pch.h"
#include "RenderBackend.h"

#include <vulkan/vulkan.h>

#include "SpirvReflection.h"
#include "VertexInput.h"
#include "ShaderIO.h"
#include "Utilities/GeneralUtils.h"
#include "Utilities/MathUtils.h"
#include "VertexInputVulkan.h"
#include "VulkanImageFormats.h"
#include "Runtime/Timer.h"
#include "JobSystem.h"
#include "VulkanImage.h"
#include "../../Window.h"
#include "Framebuffer.h"
#include "RenderPass.h"

// disable ImGui warnings
#pragma warning( push )
#pragma warning( disable : 26495 26812)

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>

// definition of extern variable from header
RenderBackend gRenderBackend;

// reenable warnings
#pragma warning( pop )

// vulkan uses enums, which result in a warning every time they are used
// this warning is disabled for this entire file
#pragma warning( disable : 26812) //C26812: Prefer 'enum class' over 'enum' 

const uint32_t maxTextureCount = 1000;

void RenderBackend::setup(GLFWwindow* window) {

    m_shaderFileManager.setup();

    createVulkanInstance();
    createSurface(window);
    pickPhysicalDevice(m_swapchain.surface);

    VkPhysicalDeviceProperties deviceProperties = getVulkanDeviceProperties();
    m_nanosecondsPerTimestamp = deviceProperties.limits.timestampPeriod;

    std::cout << "Picked physical device: " << deviceProperties.deviceName << std::endl;
    std::cout << std::endl;

    getQueueFamilies(vkContext.physicalDevice, &vkContext.queueFamilies, m_swapchain.surface);
    createLogicalDevice();
    initializeVulkanQueues();
    chooseSurfaceFormat();
    createSwapChain();

    const std::array<int, 2> resolution = Window::getGlfwWindowResolution(window);
    initSwapchainImages((uint32_t)resolution[0], (uint32_t)resolution[1]);

    acquireDebugUtilsExtFunctionsPointers();

    m_vkAllocator.create();
    m_commandPool = createCommandPool(vkContext.queueFamilies.graphicsQueueIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    m_drawcallCommandPools = createDrawcallCommandPools();
    m_transientCommandPool = createCommandPool(vkContext.queueFamilies.transferQueueFamilyIndex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    m_swapchain.imageAvaible = createSemaphore();
    m_renderFinishedSemaphore = createSemaphore();
    m_renderFinishedFence = createFence();
    m_stagingBuffer = createStagingBuffer();
    m_commandBuffers[0] = allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, m_commandPool);
    m_commandBuffers[1] = allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, m_commandPool);

    initGlobalTextureArrayDescriptorSetLayout();
    initGlobalTextureArrayDescriptorSet();
    setupImgui(window);

    m_timestampQueryPools[0] = createQueryPool(VK_QUERY_TYPE_TIMESTAMP, m_timestampQueryPoolQueryCount);
    m_timestampQueryPools[1] = createQueryPool(VK_QUERY_TYPE_TIMESTAMP, m_timestampQueryPoolQueryCount);
}

Buffer RenderBackend::createStagingBuffer() {
    const auto stagingBufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const auto stagingBufferMemoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const std::vector<uint32_t> stagingBufferQueueFamilies = { vkContext.queueFamilies.transferQueueFamilyIndex };
    return createBufferInternal(
        m_stagingBufferSize,
        stagingBufferQueueFamilies,
        stagingBufferUsageFlags,
        stagingBufferMemoryFlags);
}

std::vector<VkCommandPool> RenderBackend::createDrawcallCommandPools() {
    std::vector<VkCommandPool> drawcallPools;
    for (int i = 0; i < JobSystem::getWorkerCount(); i++) {
        const VkCommandPool pool = createCommandPool(vkContext.queueFamilies.graphicsQueueIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        drawcallPools.push_back(pool);
    }
    return drawcallPools;
}

void RenderBackend::acquireDebugUtilsExtFunctionsPointers() {
    m_debugExtFunctions.vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(vkContext.device, "vkCmdBeginDebugUtilsLabelEXT");
    m_debugExtFunctions.vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(vkContext.device, "vkCmdEndDebugUtilsLabelEXT");
    m_debugExtFunctions.vkCmdInsertDebugUtilsLabelEXT = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetDeviceProcAddr(vkContext.device, "vkCmdInsertDebugUtilsLabelEXT");
    m_debugExtFunctions.vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetDeviceProcAddr(vkContext.device, "vkCreateDebugUtilsMessengerEXT");
    m_debugExtFunctions.vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetDeviceProcAddr(vkContext.device, "vkDestroyDebugUtilsMessengerEXT");
    m_debugExtFunctions.vkQueueBeginDebugUtilsLabelEXT = (PFN_vkQueueBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(vkContext.device, "vkQueueBeginDebugUtilsLabelEXT");
    m_debugExtFunctions.vkQueueEndDebugUtilsLabelEXT = (PFN_vkQueueEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(vkContext.device, "vkQueueEndDebugUtilsLabelEXT");
    m_debugExtFunctions.vkQueueInsertDebugUtilsLabelEXT = (PFN_vkQueueInsertDebugUtilsLabelEXT)vkGetDeviceProcAddr(vkContext.device, "vkQueueInsertDebugUtilsLabelEXT");
    m_debugExtFunctions.vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(vkContext.device, "vkSetDebugUtilsObjectNameEXT");
    m_debugExtFunctions.vkSetDebugUtilsObjectTagEXT = (PFN_vkSetDebugUtilsObjectTagEXT)vkGetDeviceProcAddr(vkContext.device, "vkSetDebugUtilsObjectTagEXT");
    m_debugExtFunctions.vkSubmitDebugUtilsMessageEXT = (PFN_vkSubmitDebugUtilsMessageEXT)vkGetDeviceProcAddr(vkContext.device, "vkSubmitDebugUtilsMessageEXT");
}

void RenderBackend::shutdown() {

    waitForRenderFinished();
    m_shaderFileManager.shutdown();

    for (uint32_t i = 0; i < m_images.size(); i++) {
        destroyImage({ i });
    }
    for (const AllocatedTempImage& tempImage : m_allocatedTempImages) {
        destroyImageInternal(tempImage.image);
    }
    for (uint32_t i = 0; i < m_renderPasses.getGraphicPassCount(); i++) {
        destroyGraphicPass(m_renderPasses.getGraphicPassRefByIndex(i));
    }
    for (uint32_t i = 0; i < m_renderPasses.getComputePassCount(); i++) {
        destroyComputePass(m_renderPasses.getComputePassRefByIndex(i));
    }
    for (const auto& mesh : m_meshes) {
        destroyMesh(mesh);
    }
    for (const auto& buffer : m_uniformBuffers) {
        destroyBuffer(buffer);
    }
    for (const auto& buffer : m_storageBuffers) {
        destroyBuffer(buffer);
    }
    for (const auto& sampler : m_samplers) {
        vkDestroySampler(vkContext.device, sampler, nullptr);
    }
    destroyFramebuffers(m_transientFramebuffers[0]);
    destroyFramebuffers(m_transientFramebuffers[1]);

    vkDestroyDescriptorSetLayout(vkContext.device, m_globalTextureArrayDescriporSetLayout, nullptr);

    //destroy swapchain
    vkDestroySwapchainKHR(vkContext.device, m_swapchain.vulkanHandle, nullptr);
    vkDestroySurfaceKHR(vkContext.vulkanInstance, m_swapchain.surface, nullptr);

    m_vkAllocator.destroy();

    destroyFramebuffers(m_ui.framebuffers);
    vkDestroyRenderPass(vkContext.device, m_ui.renderPass, nullptr);
    ImGui_ImplVulkan_Shutdown();

    destroyBuffer(m_stagingBuffer);

    for (const auto& pool : m_descriptorPools) {
        vkDestroyDescriptorPool(vkContext.device, pool.vkPool, nullptr);
    }
    vkDestroyDescriptorPool(vkContext.device, m_imguiDescriptorPool, nullptr);
    vkDestroyDescriptorPool(vkContext.device, m_globalTextureArrayDescriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(vkContext.device, m_globalDescriptorSetLayout, nullptr);

    vkDestroyCommandPool(vkContext.device, m_commandPool, nullptr);
    vkDestroyCommandPool(vkContext.device, m_transientCommandPool, nullptr);

    for (const VkCommandPool pool : m_drawcallCommandPools) {
        vkDestroyCommandPool(vkContext.device, pool, nullptr);
    }

    vkDestroySemaphore(vkContext.device, m_renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(vkContext.device, m_swapchain.imageAvaible, nullptr);

    vkDestroyQueryPool(vkContext.device, m_timestampQueryPools[0], nullptr);
    vkDestroyQueryPool(vkContext.device, m_timestampQueryPools[1], nullptr);

    vkDestroyFence(vkContext.device, m_renderFinishedFence, nullptr);
    vkDestroyDevice(vkContext.device, nullptr);
    destroyVulkanInstance();
}

void RenderBackend::recreateSwapchain(const uint32_t width, const uint32_t height, GLFWwindow* window) {

    auto result = vkDeviceWaitIdle(vkContext.device);
    checkVulkanResult(result);

    for (const auto& imageHandle : m_swapchain.imageHandles) {
        destroyImage(imageHandle);
    }
    vkDestroySwapchainKHR(vkContext.device, m_swapchain.vulkanHandle, nullptr);
    vkDestroySurfaceKHR(vkContext.vulkanInstance, m_swapchain.surface, nullptr);
    
    createSurface(window);

    // queue families must revalidate present support for new surface
    getQueueFamilies(vkContext.physicalDevice, &vkContext.queueFamilies, m_swapchain.surface);
    createSwapChain();
    initSwapchainImages(width, height);

    destroyFramebuffers(m_ui.framebuffers);
    m_ui.framebuffers = createImGuiFramebuffers();
    m_ui.passBeginInfos = createImGuiPassBeginInfo(width, height);
}

void RenderBackend::updateShaderCode() {

    const std::vector<ComputePassShaderReloadInfo> computeShadersReloadInfos = m_shaderFileManager.reloadOutOfDateComputeShaders();
    const std::vector<GraphicPassShaderReloadInfo> graphicShadersReloadInfos = m_shaderFileManager.reloadOutOfDateGraphicShaders();

    if (computeShadersReloadInfos.size() == 0 && graphicShadersReloadInfos.size() == 0) {
        return;
    }

    //when updating passes they must not be used
    auto result = vkDeviceWaitIdle(vkContext.device);
    checkVulkanResult(result);

    //recreate compute passes
    for (const ComputePassShaderReloadInfo& reloadInfo : computeShadersReloadInfos) {
        ComputePass& pass = m_renderPasses.getComputePassRefByHandle(reloadInfo.renderpass);
        ComputeShaderHandle shaderHandle = pass.shaderHandle;
        destroyComputePass(pass);
        pass = createComputePassInternal(pass.computePassDesc, reloadInfo.spirV);
        pass.shaderHandle = shaderHandle;
    }

    //recreate graphic passes
    for (const GraphicPassShaderReloadInfo& reloadInfo : graphicShadersReloadInfos) {
        GraphicPass& pass = m_renderPasses.getGraphicPassRefByHandle(reloadInfo.renderpass);
        const GraphicShadersHandle shaderHandle = pass.shaderHandle;
        destroyGraphicPass(pass);
        pass = createGraphicPassInternal(pass.graphicPassDesc, reloadInfo.spirV);
        pass.shaderHandle = shaderHandle;
    }
}

void RenderBackend::resizeImages(const std::vector<ImageHandle>& images, const uint32_t width, const uint32_t height) {

    //recreate image    
    for (const auto image : images) {
        m_images[image.index].desc.width = width;
        m_images[image.index].desc.height = height;
        const auto imageDesc = m_images[image.index].desc;
        destroyImage(image);
        ImageHandle newHandle = createImage(imageDesc, nullptr, 0);
        assert(newHandle.index == image.index);
    }

    //recreate framebuffer that use image    
    VkExtent2D extent;
    extent.width = width;
    extent.height = height;

    VkRect2D rect = {};
    rect.extent = extent;
    rect.offset = { 0, 0 };
}

void RenderBackend::newFrame() {

    m_graphicPassExecutions.clear();
    m_computePassExecutions.clear();
    m_renderPassExecutions.clear();
    m_temporaryImages.clear();
    resetAllocatedTempImages();

    m_swapchainInputImageHandle.index = 0;

    m_frameIndex++;
    m_frameIndexMod2 = m_frameIndex % 2;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void RenderBackend::prepareForDrawcallRecording() {
    allocateTemporaryImages();
    updateRenderPassDescriptorSets();
    destroyFramebuffers(m_transientFramebuffers[m_frameIndexMod2]);
    m_transientFramebuffers[m_frameIndexMod2] = createGraphicPassFramebuffer(m_graphicPassExecutions);
    startGraphicPassRecording();
}

void RenderBackend::setGraphicPassExecution(const GraphicPassExecution& execution) {
    RenderPassExecutionEntry executionEntry;
    executionEntry.index = m_graphicPassExecutions.size();
    executionEntry.type = RenderPassType::Graphic;
    m_renderPassExecutions.push_back(executionEntry);
    m_graphicPassExecutions.push_back(execution);
}

void RenderBackend::setComputePassExecution(const ComputePassExecution& execution) {
    RenderPassExecutionEntry executionEntry;
    executionEntry.index = m_computePassExecutions.size();
    executionEntry.type = RenderPassType::Compute;
    m_renderPassExecutions.push_back(executionEntry);
    m_computePassExecutions.push_back(execution);
}

void RenderBackend::drawMeshes(const std::vector<MeshHandle> meshHandles, const char* pushConstantData,const RenderPassHandle passHandle, const int workerIndex) {
    const GraphicPass& pass = m_renderPasses.getGraphicPassRefByHandle(passHandle);

    VkShaderStageFlags pushConstantStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    if (pass.graphicPassDesc.shaderDescriptions.geometry.has_value()) {
        pushConstantStageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    }

    const int poolCount = m_drawcallCommandPools.size();
    const int poolIndex = workerIndex + poolCount * m_frameIndexMod2;
    
    const VkCommandBuffer meshCommandBuffer = pass.meshCommandBuffers[poolIndex];
    const VkDescriptorSet sets[3] = { m_globalDescriptorSet, pass.descriptorSets[m_frameIndexMod2], m_globalTextureArrayDescriptorSet };
    vkCmdBindDescriptorSets(meshCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipelineLayout, 0, 3, sets, 0, nullptr);

    for (uint32_t i = 0; i < meshHandles.size(); i++) {

        const Mesh mesh = m_meshes[meshHandles[i].index];

        //vertex/index buffers            
        VkDeviceSize offset[] = { 0 };
        vkCmdBindVertexBuffers(meshCommandBuffer, 0, 1, &mesh.vertexBuffer.vulkanHandle, offset);
        vkCmdBindIndexBuffer(meshCommandBuffer, mesh.indexBuffer.vulkanHandle, offset[0], mesh.indexPrecision);

    if (pass.pushConstantSize > 0) {
        //update push constants
        vkCmdPushConstants(
            meshCommandBuffer,
            pass.pipelineLayout,
            pushConstantStageFlags,
            0,
            (uint32_t)pass.pushConstantSize,
            pushConstantData + i * pass.pushConstantSize);
    }
    vkCmdDrawIndexed(meshCommandBuffer, mesh.indexCount, 1, 0, 0, 0);
    }
}

void RenderBackend::setUniformBufferData(const UniformBufferHandle buffer, const void* data, const size_t size) {
    m_deferredUniformBufferFills.emplace_back(UniformBufferFillOrder{ buffer, dataToCharArray(data, size), });
}

void RenderBackend::setStorageBufferData(const StorageBufferHandle buffer, const void* data, const size_t size) {
    m_deferredStorageBufferFills.emplace_back(StorageBufferFillOrder{ buffer, dataToCharArray(data, size), });
}

void RenderBackend::setGlobalDescriptorSetLayout(const ShaderLayout& layout) {
    if (m_globalDescriptorSetLayout != VK_NULL_HANDLE) {
        std::cout << "Error: global descriptor set layout must only be set once\n";
    }
    m_globalDescriptorSetLayout = createDescriptorSetLayout(layout);
    const DescriptorPoolAllocationSizes setSizes = descriptorSetAllocationSizeFromShaderLayout(layout);
    m_globalDescriptorSet = allocateDescriptorSet(m_globalDescriptorSetLayout, setSizes);
}

void RenderBackend::setGlobalDescriptorSetResources(const RenderPassResources& resources) {
    updateDescriptorSet(m_globalDescriptorSet, resources);
}

void RenderBackend::updateGraphicPassShaderDescription(const RenderPassHandle passHandle, const GraphicPassShaderDescriptions& desc) {
    assert(getRenderPassType(passHandle) == RenderPassType::Graphic);
    GraphicPass& pass = m_renderPasses.getGraphicPassRefByHandle(passHandle);
    pass.graphicPassDesc.shaderDescriptions = desc;
    GraphicPassShaderSpirV spirV;
    if (m_shaderFileManager.loadGraphicShadersSpirV(pass.shaderHandle, &spirV)) {
        auto result = vkDeviceWaitIdle(vkContext.device);
        checkVulkanResult(result);
        const GraphicShadersHandle shaderHandle = pass.shaderHandle;
        destroyGraphicPass(pass);
        pass = createGraphicPassInternal(pass.graphicPassDesc, spirV);
        pass.shaderHandle = shaderHandle;
    }
}

void RenderBackend::updateComputePassShaderDescription(const RenderPassHandle passHandle, const ShaderDescription& desc) {
    assert(getRenderPassType(passHandle) == RenderPassType::Compute);
    ComputePass& pass = m_renderPasses.getComputePassRefByHandle(passHandle);
    pass.computePassDesc.shaderDescription = desc;
    std::vector<uint32_t> spirV;
    std::vector<char> glsl;
    if (m_shaderFileManager.loadComputeShaderSpirV(pass.shaderHandle, &spirV)) {
        auto result = vkDeviceWaitIdle(vkContext.device);
        checkVulkanResult(result);
        const ComputeShaderHandle shaderHandle = pass.shaderHandle;
        destroyComputePass(pass);
        pass = createComputePassInternal(pass.computePassDesc, spirV);
        pass.shaderHandle = shaderHandle;
    }
}

//helper that picks correct vector from entry and returns RenderPassExecution accprdong to entry index
RenderPassExecution getGenericRenderpassInfoFromExecutionEntry(const RenderPassExecutionEntry& entry,
    const std::vector<GraphicPassExecution>& graphicExecutions,
    const std::vector<ComputePassExecution>& computeExecutions) {
    
    if (entry.type == RenderPassType::Graphic) {
        return graphicExecutions[entry.index].genericInfo;
    }
    else if (entry.type == RenderPassType::Compute) {
        return computeExecutions[entry.index].genericInfo;
    }
    else {
        std::cout << "Unknown RenderPassType\n";
        throw("Unknown RenderPassType");
    }
}

void RenderBackend::renderFrame(const bool presentToScreen) {

    //reset doesn't work before waiting for render finished fence
    resetTimestampQueryPool(m_frameIndexMod2);
    
    //record command buffer
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    
    const VkCommandBuffer currentCommandBuffer = m_commandBuffers[m_frameIndexMod2];
    
    auto res = vkResetCommandBuffer(currentCommandBuffer, 0);
    assert(res == VK_SUCCESS);
    res = vkBeginCommandBuffer(currentCommandBuffer, &beginInfo);
    assert(res == VK_SUCCESS);
    
    //index needed for end query
    const uint32_t frameQueryIndex = (uint32_t)m_timestampQueriesPerFrame[m_frameIndexMod2].size();
    {
        TimestampQuery frameQuery;
        frameQuery.name = "Frame";
        frameQuery.startQuery = issueTimestampQuery(currentCommandBuffer, m_frameIndexMod2);

        m_timestampQueriesPerFrame[m_frameIndexMod2].push_back(frameQuery);
    }

    const std::vector<RenderPassBarriers> barriers = createRenderPassBarriers();

    int graphicPassIndex = 0;
    for (int i = 0; i < m_renderPassExecutions.size(); i++) {
        const RenderPassExecutionEntry& executionEntry = m_renderPassExecutions[i];
        if (executionEntry.type == RenderPassType::Graphic) {
            const VkFramebuffer f = m_transientFramebuffers[m_frameIndexMod2][graphicPassIndex];
            submitGraphicPass(m_graphicPassExecutions[executionEntry.index], barriers[i], 
                currentCommandBuffer, f);
            graphicPassIndex++;
        }
        else if (executionEntry.type == RenderPassType::Compute) {
            submitComputePass(m_computePassExecutions[executionEntry.index], barriers[i], currentCommandBuffer);
        }
        else {
            std::cout << "Unknown RenderPassType\n";
        }
    }

    //imgui
    {
        startDebugLabel(currentCommandBuffer, "ImGui");
    
        TimestampQuery imguiQuery;
        imguiQuery.name = "ImGui";
        imguiQuery.startQuery = issueTimestampQuery(currentCommandBuffer, m_frameIndexMod2);
    
        ImGui::Render();

        const std::vector<VkImageMemoryBarrier> uiBarrier = createImageBarriers(m_images[m_swapchainInputImageHandle.index],
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, 1);

        vkCmdPipelineBarrier(currentCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0, 0, nullptr, 0, nullptr, 1, uiBarrier.data());
    
        vkCmdBeginRenderPass(currentCommandBuffer, &m_ui.passBeginInfos[m_swapchainInputImageIndex], VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), currentCommandBuffer);
        vkCmdEndRenderPass(currentCommandBuffer);
    
        imguiQuery.endQuery = issueTimestampQuery(currentCommandBuffer, m_frameIndexMod2);
        m_timestampQueriesPerFrame[m_frameIndexMod2].push_back(imguiQuery);
    
        endDebugLabel(currentCommandBuffer);
    }
    
    m_timestampQueriesPerFrame[m_frameIndexMod2][frameQueryIndex].endQuery = issueTimestampQuery(currentCommandBuffer, m_frameIndexMod2);
    
    //transition swapchain image to present
    auto& swapchainPresentImage = m_images[m_swapchainInputImageHandle.index];
    const auto& transitionToPresentBarrier = createImageBarriers(swapchainPresentImage, 
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, 1);
    barriersCommand(currentCommandBuffer, transitionToPresentBarrier, std::vector<VkBufferMemoryBarrier> {});

    res = vkEndCommandBuffer(currentCommandBuffer);
    assert(res == VK_SUCCESS);

    //compute cpu time after drawcall recording, but before waiting for GPU to finish
    m_lastFrameCPUTime = Timer::getTimeFloat() - m_timeOfLastGPUSubmit;
    
    //wait for in flight frame to render so resources are avaible
    res = vkWaitForFences(vkContext.device, 1, &m_renderFinishedFence, VK_TRUE, UINT64_MAX);
    assert(res == VK_SUCCESS);
    res = vkResetFences(vkContext.device, 1, &m_renderFinishedFence);
    assert(res == VK_SUCCESS);
    
    //execute deferred buffer fill orders
    for (const UniformBufferFillOrder& order : m_deferredUniformBufferFills) {
        fillBuffer(m_uniformBuffers[order.buffer.index], order.data.data(), order.data.size());
    }
    for (const StorageBufferFillOrder& order : m_deferredStorageBufferFills) {
        fillBuffer(m_storageBuffers[order.buffer.index], order.data.data(), order.data.size());
    }
    m_deferredUniformBufferFills.clear();
    m_deferredStorageBufferFills.clear();

    //submit 
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = nullptr;
    submit.waitSemaphoreCount = presentToScreen ? 1 : 0;
    submit.pWaitSemaphores = &m_swapchain.imageAvaible;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit.pWaitDstStageMask = &waitStage;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &currentCommandBuffer;
    submit.signalSemaphoreCount = presentToScreen ? 1 : 0;;
    submit.pSignalSemaphores = &m_renderFinishedSemaphore;

    res = vkQueueSubmit(vkContext.graphicQueue, 1, &submit, m_renderFinishedFence);
    assert(res == VK_SUCCESS);

    if (presentToScreen) {
        presentImage(m_renderFinishedSemaphore);
        glfwPollEvents();
    }

    //timestamp after presenting, which waits for vsync
    m_timeOfLastGPUSubmit = Timer::getTimeFloat();

    //get timestamp results of last frame
    {
        m_renderpassTimings.clear();
        
        const int previousFrameIndexMod2 = (m_frameIndexMod2 + 1) % 2;
        const size_t previousFrameQueryCount = m_timestampQueryCounts[previousFrameIndexMod2];
        const VkQueryPool previousQueryPool = m_timestampQueryPools[previousFrameIndexMod2];
        
        std::vector<uint32_t> timestamps;
        timestamps.resize(previousFrameQueryCount);
        
        //res = vkGetQueryPoolResults(vkContext.device, m_timestampQueryPool, 0, m_currentTimestampQueryCount,
        //    timestamps.size() * sizeof(uint32_t), timestamps.data(), 0, VK_QUERY_RESULT_WAIT_BIT);
        //assert(res == VK_SUCCESS);
        //on Ryzen 4700U iGPU vkGetQueryPoolResults only returns correct results for the first query
        //maybe it contains more info so needs more space per query?
        //manually get every query for now
        //FIXME: proper solution
        for (size_t i = 0; i < previousFrameQueryCount; i++) {
            auto result = vkGetQueryPoolResults(vkContext.device, previousQueryPool, (uint32_t)i, 1,
                (uint32_t)timestamps.size() * sizeof(uint32_t), &timestamps[i], 0, VK_QUERY_RESULT_WAIT_BIT);
            checkVulkanResult(result);
        }
        
        for (const TimestampQuery query : m_timestampQueriesPerFrame[previousFrameIndexMod2]) {

            const uint32_t startTime = timestamps[query.startQuery];
            const uint32_t endTime = timestamps[query.endQuery];
            const uint32_t time = endTime - startTime;

            const float nanoseconds = (float)time * m_nanosecondsPerTimestamp;
            const float milliseconds = nanoseconds * 0.000001f;

            RenderPassTime timing;
            timing.name = query.name;
            timing.timeMs = milliseconds;
            m_renderpassTimings.push_back(timing);
        }
    }
}

uint32_t RenderBackend::getImageGlobalTextureArrayIndex(const ImageHandle image) {
    return getImageRef(image).globalDescriptorSetIndex;
}

RenderPassHandle RenderBackend::createComputePass(const ComputePassDescription& desc) {

    const ComputeShaderHandle shaderHandle = m_shaderFileManager.addComputeShader(desc.shaderDescription);

    std::vector<uint32_t> spirV;
    if (!m_shaderFileManager.loadComputeShaderSpirV(shaderHandle, &spirV)) {
        std::cout << "Initial shader loading failed" << std::endl; //loadShaders provides error details trough cout
        throw;
    }

    ComputePass pass = createComputePassInternal(desc, spirV);
    pass.shaderHandle = shaderHandle;
    RenderPassHandle passHandle = m_renderPasses.addComputePass(pass);
    m_shaderFileManager.setComputePassHandle(shaderHandle, passHandle);
    return passHandle;
}

RenderPassHandle RenderBackend::createGraphicPass(const GraphicPassDescription& desc) {

    const GraphicShadersHandle shaderHandle = m_shaderFileManager.addGraphicShaders(desc.shaderDescriptions);

    GraphicPassShaderSpirV spirV;
    if (!m_shaderFileManager.loadGraphicShadersSpirV(shaderHandle, &spirV)) {
        std::cout << "Initial shader loading failed" << std::endl;
        throw;
    }

    //create vulkan pass and handle
    GraphicPass pass = createGraphicPassInternal(desc, spirV);
    pass.shaderHandle = shaderHandle;
    RenderPassHandle passHandle = m_renderPasses.addGraphicPass(pass); 
    m_shaderFileManager.setGraphicPassHandle(shaderHandle, passHandle);
    return passHandle;
}

std::vector<MeshHandle> RenderBackend::createMeshes(const std::vector<MeshBinary>& meshes) {

    std::vector<MeshHandle> handles;
    for (uint32_t i = 0; i < meshes.size(); i++) {

        const MeshBinary& meshData = meshes[i];
        std::vector<uint32_t> queueFamilies = { vkContext.queueFamilies.graphicsQueueIndex };

        Mesh mesh;
        mesh.indexCount = meshData.indexCount;

        //index buffer
        if (mesh.indexCount < std::numeric_limits<uint16_t>::max()) {
            mesh.indexPrecision = VK_INDEX_TYPE_UINT16;
        }
        else {
            mesh.indexPrecision = VK_INDEX_TYPE_UINT32;
        }

        const VkDeviceSize indexBufferSize = meshData.indexBuffer.size() * sizeof(uint16_t);
        mesh.indexBuffer = createBufferInternal(
            indexBufferSize, 
            queueFamilies, 
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        fillBuffer(mesh.indexBuffer, meshData.indexBuffer.data(), indexBufferSize);

        //vertex buffer
        const VkDeviceSize vertexBufferSize = meshData.vertexBuffer.size() * sizeof(uint8_t);
        mesh.vertexBuffer = createBufferInternal(
            vertexBufferSize, 
            queueFamilies,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        fillBuffer(mesh.vertexBuffer, meshData.vertexBuffer.data(), vertexBufferSize);

        //store and return handle
        MeshHandle handle = { (uint32_t)m_meshes.size() };
        handles.push_back(handle);
        m_meshes.push_back(mesh);
    }
    return handles;
}

ImageHandle RenderBackend::createImage(const ImageDescription& desc, const void* initialData, const size_t initialDataSize) {

    const Image image = createImageInternal(desc, initialData, initialDataSize);

    //reuse a free image handle or create a new one
    ImageHandle handle;
    if (m_freeImageHandles.size() > 0) {
        handle = m_freeImageHandles.back();
        m_freeImageHandles.pop_back();
        m_images[handle.index] = image;
    }
    else {
        handle.index = (uint32_t)m_images.size();
        m_images.push_back(image);
    }
    return handle;
}

UniformBufferHandle RenderBackend::createUniformBuffer(const UniformBufferDescription& desc) {

    std::vector<uint32_t> queueFamilies = {
        vkContext.queueFamilies.transferQueueFamilyIndex,
        vkContext.queueFamilies.graphicsQueueIndex,
        vkContext.queueFamilies.computeQueueIndex };

    Buffer uniformBuffer = createBufferInternal(desc.size, queueFamilies,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (desc.initialData != nullptr) {
        fillBuffer(uniformBuffer, desc.initialData, desc.size);
    }

    UniformBufferHandle handle = { (uint32_t)m_uniformBuffers.size() };
    m_uniformBuffers.push_back(uniformBuffer);
    return handle;
}

StorageBufferHandle RenderBackend::createStorageBuffer(const StorageBufferDescription& desc) {

    std::vector<uint32_t> queueFamilies = {
        vkContext.queueFamilies.transferQueueFamilyIndex,
        vkContext.queueFamilies.graphicsQueueIndex,
        vkContext.queueFamilies.computeQueueIndex};

    Buffer storageBuffer = createBufferInternal(desc.size, queueFamilies, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (desc.initialData != nullptr) {
        fillBuffer(storageBuffer, desc.initialData, desc.size);
    }

    StorageBufferHandle handle = { (uint32_t)m_storageBuffers.size() };
    m_storageBuffers.push_back(storageBuffer);
    return handle;
}

SamplerHandle RenderBackend::createSampler(const SamplerDescription& desc) {

    //TODO proper min and mag filters
    //TODO allow unnormalized coordinates
    VkFilter filter;
    switch (desc.interpolation) {
    case(SamplerInterpolation::Linear): filter = VK_FILTER_LINEAR; break;
    case(SamplerInterpolation::Nearest): filter = VK_FILTER_NEAREST; break;
    default: throw std::runtime_error("unsupported sampler interpolation");
    }

    VkSamplerAddressMode wrapping;
    switch (desc.wrapping) {
    case(SamplerWrapping::Clamp): wrapping = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; break;
    case(SamplerWrapping::Color): wrapping = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; break;
    case(SamplerWrapping::Repeat): wrapping = VK_SAMPLER_ADDRESS_MODE_REPEAT; break;
    default: throw std::runtime_error("unsupported sampler wrapping mode");
    }

    VkBorderColor borderColor;
    switch (desc.borderColor) {
    case(SamplerBorderColor::Black): borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK; break;
    case(SamplerBorderColor::White): borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; break;
    default: throw std::runtime_error("unsupported sampler border color");
    }

    VkSamplerCreateInfo samplerInfo;
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.pNext = nullptr;
    samplerInfo.flags = 0;
    samplerInfo.magFilter = filter;
    samplerInfo.minFilter = filter;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = wrapping;
    samplerInfo.addressModeV = wrapping;
    samplerInfo.addressModeW = wrapping;
    samplerInfo.mipLodBias = 0.f;
    samplerInfo.anisotropyEnable = desc.useAnisotropy ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = 8.f;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.minLod = 0.f;
    samplerInfo.maxLod = (float)desc.maxMip;
    samplerInfo.borderColor = borderColor;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    VkSampler sampler;
    const auto result = vkCreateSampler(vkContext.device, &samplerInfo, nullptr, &sampler);
    checkVulkanResult(result);

    SamplerHandle handle = { (uint32_t)m_samplers.size() };
    m_samplers.push_back(sampler);
    return handle;
}

ImageHandle RenderBackend::createTemporaryImage(const ImageDescription& description) {
    ImageHandle handle;
    handle.index = m_temporaryImages.size();
    handle.index |= 0x80000000;

    TemporaryImage tempImage;
    tempImage.desc = description;
    m_temporaryImages.push_back(tempImage);

    return handle;
}

ImageHandle RenderBackend::getSwapchainInputImage() {
    auto result = vkAcquireNextImageKHR(vkContext.device, m_swapchain.vulkanHandle, UINT64_MAX, m_swapchain.imageAvaible, VK_NULL_HANDLE, &m_swapchainInputImageIndex);
    checkVulkanResult(result);
    m_swapchainInputImageHandle = m_swapchain.imageHandles[m_swapchainInputImageIndex];
    return m_swapchainInputImageHandle;
}

void RenderBackend::getMemoryStats(uint64_t* outAllocatedSize, uint64_t* outUsedSize) const{
    assert(outAllocatedSize != nullptr);
    assert(outUsedSize != nullptr);
    m_vkAllocator.getMemoryStats(outAllocatedSize, outUsedSize);
    *outAllocatedSize   += (uint32_t)m_stagingBufferSize;
    *outUsedSize        += (uint32_t)m_stagingBufferSize;
}

std::vector<RenderPassTime> RenderBackend::getRenderpassTimings() const {
    return m_renderpassTimings;
}

float RenderBackend::getLastFrameCPUTime() const {
    return m_lastFrameCPUTime;
}

struct ImageHandleDecoded {
    bool isTempImage = false;
    int index = 0;
};

ImageHandleDecoded decodeImageHandle(const ImageHandle handle) {
    ImageHandleDecoded decoded;
    decoded.isTempImage = bool(handle.index >> 31); //first bit indicates if image is temp
    decoded.index = handle.index & 0x7FFFFFFF;      //mask out first bit for index
    return decoded;
}

ImageDescription RenderBackend::getImageDescription(const ImageHandle image) {
    const ImageHandleDecoded decoded = decodeImageHandle(image);
    if (decoded.isTempImage) {
        return m_temporaryImages[decoded.index].desc;
    }
    else {
        return m_images[decoded.index].desc;
    }
}

void RenderBackend::waitForGpuIdle() {
    vkDeviceWaitIdle(vkContext.device);
}

std::vector<RenderPassBarriers> RenderBackend::createRenderPassBarriers() {

    std::vector<RenderPassBarriers> barrierList;

    for (const RenderPassExecutionEntry executionEntry : m_renderPassExecutions) {

        const RenderPassExecution execution = getGenericRenderpassInfoFromExecutionEntry(executionEntry,
            m_graphicPassExecutions, m_computePassExecutions);

        const RenderPassResources& resources = execution.resources;
        RenderPassBarriers barriers;

        //storage images        
        for (const ImageResource& storageImage : resources.storageImages) {
            Image& image = getImageRef(storageImage.image);

            //check if any mip levels need a layout transition            
            const VkImageLayout requiredLayout = VK_IMAGE_LAYOUT_GENERAL;
            bool needsLayoutTransition = false;
            for (const auto& layout : image.layoutPerMip) {
                if (layout != requiredLayout) {
                    needsLayoutTransition = true;
                }
            }

            //check if image already has a barrier
            //can happen if same image is used as two storage image when accessing different mips            
            bool hasBarrierAlready = false;
            for (const auto& barrier : barriers.imageBarriers) {
                if (barrier.image == image.vulkanHandle) {
                    hasBarrierAlready = true;
                    break;
                }
            }
            
            if ((image.currentlyWriting || needsLayoutTransition) && !hasBarrierAlready) {
                const auto& layoutBarriers = createImageBarriers(image, requiredLayout,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, 0, (uint32_t)image.layoutPerMip.size());
                barriers.imageBarriers.insert(barriers.imageBarriers.end(), layoutBarriers.begin(), layoutBarriers.end());
            }
            image.currentlyWriting = true;
        }

        //sampled images
        for (const ImageResource& sampledImage : resources.sampledImages) {

            //use general layout if image is used as a storage image too
            bool isUsedAsStorageImage = false;
            {
                for (const ImageResource& storageImage : resources.storageImages) {
                    if (storageImage.image.index == sampledImage.image.index) {
                        isUsedAsStorageImage = true;
                        break;
                    }
                }
            }
            if (isUsedAsStorageImage) {
                continue;
            }

            Image& image = getImageRef(sampledImage.image);

            //check if any mip levels need a layout transition            
            VkImageLayout requiredLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            bool needsLayoutTransition = false;
            for (const auto& layout : image.layoutPerMip) {
                if (layout != requiredLayout) {
                    needsLayoutTransition = true;
                }
            }

            if (image.currentlyWriting || needsLayoutTransition) {
                const auto& layoutBarriers = createImageBarriers(image, requiredLayout, VK_ACCESS_SHADER_READ_BIT,
                    0, (uint32_t)image.viewPerMip.size());
                barriers.imageBarriers.insert(barriers.imageBarriers.end(), layoutBarriers.begin(), layoutBarriers.end());
            }
        }

        //attachments        
        if (executionEntry.type == RenderPassType::Graphic) {
            const GraphicPassExecution graphicExecutionInfo = m_graphicPassExecutions[executionEntry.index];

            for (const RenderTarget& target : graphicExecutionInfo.targets) {
                Image& image = getImageRef(target.image);

            //check if any mip levels need a layout transition                
            const VkImageLayout requiredLayout = isVulkanDepthFormat(image.format) ?
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            bool needsLayoutTransition = false;
            for (const auto& layout : image.layoutPerMip) {
                if (layout != requiredLayout) {
                    needsLayoutTransition = true;
                }
            }

            if (image.currentlyWriting || needsLayoutTransition) {
                const VkAccessFlags access = isVulkanDepthFormat(image.format) ?
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                const auto& layoutBarriers = createImageBarriers(image, requiredLayout, access, 0,
                    (uint32_t)image.viewPerMip.size());
                barriers.imageBarriers.insert(barriers.imageBarriers.end(), layoutBarriers.begin(), layoutBarriers.end());
            }
            image.currentlyWriting = true;
            }
        }

        //storage buffer barriers
        for (const auto& bufferResource : resources.storageBuffers) {
        	StorageBufferHandle handle = bufferResource.buffer;
        	Buffer& buffer = m_storageBuffers[handle.index];
        	const bool needsBarrier = buffer.isBeingWritten;
        	if (needsBarrier) {
        		VkBufferMemoryBarrier barrier = createBufferBarrier(buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        		barriers.memoryBarriers.push_back(barrier);
        	}
        
        	//update writing state
        	buffer.isBeingWritten = !bufferResource.readOnly;
        }
        barrierList.push_back(barriers);
    }
    return barrierList;
}

VkRenderPassBeginInfo createBeginInfo(const uint32_t width, const uint32_t height, const VkRenderPass pass, 
    const VkFramebuffer framebuffer, const std::vector<VkClearValue>& clearValues) {

    VkExtent2D extent = {};
    extent.width = width;
    extent.height = height;

    VkRect2D rect = {};
    rect.extent = extent;
    rect.offset = { 0, 0 };

    VkRenderPassBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.renderPass = pass;
    beginInfo.framebuffer = framebuffer;
    beginInfo.clearValueCount = (uint32_t)clearValues.size();
    beginInfo.pClearValues = clearValues.data();
    beginInfo.renderArea = rect;

    return beginInfo;
}

void RenderBackend::submitGraphicPass(const GraphicPassExecution& execution,
    const RenderPassBarriers& barriers, const VkCommandBuffer commandBuffer, const VkFramebuffer framebuffer) {

    GraphicPass& pass = m_renderPasses.getGraphicPassRefByHandle(execution.genericInfo.handle);
    startDebugLabel(commandBuffer, pass.graphicPassDesc.name);

    TimestampQuery timeQuery;
    timeQuery.name = pass.graphicPassDesc.name;
    timeQuery.startQuery = issueTimestampQuery(commandBuffer, m_frameIndexMod2);

    barriersCommand(commandBuffer, barriers.imageBarriers, barriers.memoryBarriers);

    const glm::ivec2 resolution = getResolutionFromRenderTargets(execution.targets);
    const auto beginInfo = createBeginInfo(resolution.x, resolution.y, pass.vulkanRenderPass, 
        framebuffer, pass.clearValues);

    //prepare pass
    vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    const int poolCount = m_drawcallCommandPools.size();
    for (int poolIndex = 0; poolIndex < poolCount; poolIndex++) {
        const int cmdBufferIndex = poolIndex + m_frameIndexMod2 * poolCount;
        const VkCommandBuffer meshCommandBuffer = pass.meshCommandBuffers[cmdBufferIndex];

        //stop recording mesh commands
        vkEndCommandBuffer(meshCommandBuffer);
    }
    //execute mesh commands
    const int cmdBufferIndexOffset = m_frameIndexMod2 * poolCount;
    vkCmdExecuteCommands(commandBuffer, poolCount, &pass.meshCommandBuffers[cmdBufferIndexOffset]);

    vkCmdEndRenderPass(commandBuffer);

    timeQuery.endQuery = issueTimestampQuery(commandBuffer, m_frameIndexMod2);
    endDebugLabel(commandBuffer);
    m_timestampQueriesPerFrame[m_frameIndexMod2].push_back(timeQuery);
}

void RenderBackend::submitComputePass(const ComputePassExecution& execution,
    const RenderPassBarriers& barriers, const VkCommandBuffer commandBuffer) {

    TimestampQuery timeQuery;

    //TODO: add push constants for compute passes
    ComputePass& pass = m_renderPasses.getComputePassRefByHandle(execution.genericInfo.handle);
    startDebugLabel(commandBuffer, pass.computePassDesc.name);

    timeQuery.name = pass.computePassDesc.name;
    timeQuery.startQuery = issueTimestampQuery(commandBuffer, m_frameIndexMod2);

    barriersCommand(commandBuffer, barriers.imageBarriers, barriers.memoryBarriers);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipeline);

    const VkDescriptorSet sets[3] = { m_globalDescriptorSet, pass.descriptorSets[m_frameIndexMod2], m_globalTextureArrayDescriptorSet };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipelineLayout, 0, 3, sets, 0, nullptr);

    if (execution.pushConstants.size() > 0) {
        vkCmdPushConstants(
            commandBuffer, 
            pass.pipelineLayout, 
            VK_SHADER_STAGE_COMPUTE_BIT, 
            0,
            sizeof(char) * (uint32_t)execution.pushConstants.size(), 
            execution.pushConstants.data());
    }

    vkCmdDispatch(commandBuffer, execution.dispatchCount[0], execution.dispatchCount[1], execution.dispatchCount[2]);

    timeQuery.endQuery = issueTimestampQuery(commandBuffer, m_frameIndexMod2);
    endDebugLabel(commandBuffer);
    m_timestampQueriesPerFrame[m_frameIndexMod2].push_back(timeQuery);
}

void RenderBackend::waitForRenderFinished() {
    auto result = vkWaitForFences(vkContext.device, 1, &m_renderFinishedFence, VK_TRUE, INT64_MAX);
    checkVulkanResult(result);
}

std::vector<VkFramebuffer> RenderBackend::createGraphicPassFramebuffer(const std::vector<GraphicPassExecution>& execution) {
    std::vector<VkFramebuffer> framebuffers;
    for (const GraphicPassExecution& exe : execution) {
        const GraphicPass pass = m_renderPasses.getGraphicPassRefByHandle(exe.genericInfo.handle);
        const VkFramebuffer newFramebuffer = createVulkanFramebuffer(exe.targets, pass.vulkanRenderPass);
        framebuffers.push_back(newFramebuffer);
    }
    return framebuffers;
}

VkFramebuffer RenderBackend::createVulkanFramebuffer(const std::vector<RenderTarget>& targets, const VkRenderPass renderpass) {

    if (!validateAttachments(targets)) {
        std::cout << "createVulkanFramebuffer: invalid attachments\n";
        return VK_NULL_HANDLE;
    }

    std::vector<VkImageView> views;
    for (const auto& target : targets) {
        const auto& image = getImageRef(target.image);
        const auto view = image.viewPerMip[target.mipLevel];
        views.push_back(view);
    }

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.pNext = nullptr;
    framebufferInfo.flags = 0;
    framebufferInfo.renderPass = renderpass;
    framebufferInfo.attachmentCount = (uint32_t)views.size();
    framebufferInfo.pAttachments = views.data();

    const glm::ivec2 resolution = getResolutionFromRenderTargets(targets);
    framebufferInfo.width = resolution.x;
    framebufferInfo.height = resolution.y;
    framebufferInfo.layers = 1;

    VkFramebuffer framebuffer;
    auto res = vkCreateFramebuffer(vkContext.device, &framebufferInfo, nullptr, &framebuffer);
    checkVulkanResult(res);

    return framebuffer;
}

bool RenderBackend::validateAttachments(const std::vector<RenderTarget>& targets) {

    const std::string failureMessagePrologue = "Attachment validation failed: ";
    if (targets.size() == 0) {
        std::cout << failureMessagePrologue << "no attachments\n";
        return false;
    }

    glm::uvec2 resolution = getResolutionFromRenderTargets(targets);

    for (const auto attachmentDefinition : targets) {

        const Image& attachment = getImageRef(attachmentDefinition.image);

        if (!imageHasAttachmentUsageFlag(attachment)) {
            std::cout << failureMessagePrologue << "attachment image is missing attachment usage flag\n";
            return false;
        }

        const bool isResolutionMatching =
            attachment.desc.width  == resolution.x ||
            attachment.desc.height == resolution.y;
        if (!isResolutionMatching) {
            std::cout << failureMessagePrologue << "image resolutions not matching\n";
            return false;
        }

        const bool isAttachment2D = attachment.desc.depth == 1;
        if (!isAttachment2D) {
            std::cout << failureMessagePrologue << "image depth not 1, 2D image required for framebuffer\n";
            return false;
        }
    }
    return true;
}

bool RenderBackend::imageHasAttachmentUsageFlag(const Image& image) {
    const bool hasRequiredUsage = bool(image.desc.usageFlags | ImageUsageFlags::Attachment);
    return hasRequiredUsage;
}

glm::uvec2 RenderBackend::getResolutionFromRenderTargets(const std::vector<RenderTarget>& targets) {
    if (targets.size() == 0) {
        return glm::uvec2(0);
    }
    const Image& firstImage = getImageRef(targets[0].image);
    return glm::uvec2(firstImage.desc.width, firstImage.desc.height);
}

void RenderBackend::createSurface(GLFWwindow* window) {

    auto res = glfwCreateWindowSurface(vkContext.vulkanInstance, window, nullptr, &m_swapchain.surface);
    checkVulkanResult(res);
}

void RenderBackend::chooseSurfaceFormat() {

    //get avaible surface formats
    uint32_t avaibleFormatCount = 0;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(vkContext.physicalDevice, m_swapchain.surface, &avaibleFormatCount, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("failed to query surface image format count");
    }
    std::vector<VkSurfaceFormatKHR> avaibleFormats(avaibleFormatCount);
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(vkContext.physicalDevice, m_swapchain.surface, &avaibleFormatCount, avaibleFormats.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to query surface image formats");
    }

    //requested image format
    VkSurfaceFormatKHR requestedFormat = {};
    requestedFormat.format = VK_FORMAT_B8G8R8A8_UNORM; //not srgb because it doesn't support image storage
    requestedFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    //search for requested format, else keep first as fallback
    VkSurfaceFormatKHR chosenFormat = avaibleFormats[0]; //vulkan guarantess at least 1 format, so this is safe
    bool foundRequestedFormat = false;
    for (const auto& available : avaibleFormats) {
        if (available.colorSpace == requestedFormat.colorSpace && available.format == requestedFormat.format) {
            chosenFormat = available;
            foundRequestedFormat = true;
            break;
        }
    }

    if (!foundRequestedFormat) {
        std::cerr << "Warning: did not find the requested image format for swapchain" << std::endl;
    }

    m_swapchain.surfaceFormat = chosenFormat;
}

void RenderBackend::createSwapChain() {

    m_swapchain.minImageCount = 2;

    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.pNext = nullptr;
    swapchainInfo.flags = 0;
    swapchainInfo.surface = m_swapchain.surface;
    swapchainInfo.minImageCount = m_swapchain.minImageCount; //double buffered Vsync
    swapchainInfo.imageFormat = m_swapchain.surfaceFormat.format;
    swapchainInfo.imageColorSpace = m_swapchain.surfaceFormat.colorSpace;

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    auto res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkContext.physicalDevice, m_swapchain.surface, &surfaceCapabilities);
    assert(res == VK_SUCCESS);

    swapchainInfo.imageExtent = surfaceCapabilities.currentExtent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    //set sharing mode depdening on if queues are the same
    uint32_t uniqueFamilies[2] = { vkContext.queueFamilies.graphicsQueueIndex, vkContext.queueFamilies.presentationQueueIndex };
    if (uniqueFamilies[0] == uniqueFamilies[1]) {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.queueFamilyIndexCount = 0;
        swapchainInfo.pQueueFamilyIndices = nullptr;
    }
    else {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainInfo.queueFamilyIndexCount = 2;
        swapchainInfo.pQueueFamilyIndices = uniqueFamilies;
    }

    swapchainInfo.preTransform = surfaceCapabilities.currentTransform;

    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainInfo.clipped = VK_FALSE;
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

    res = vkCreateSwapchainKHR(vkContext.device, &swapchainInfo, nullptr, &m_swapchain.vulkanHandle);
    checkVulkanResult(res);
}

void RenderBackend::initSwapchainImages(const uint32_t width, const uint32_t height) {

    uint32_t swapchainImageCount = 0;
    if (vkGetSwapchainImagesKHR(vkContext.device, m_swapchain.vulkanHandle, &swapchainImageCount, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("failed to query swapchain image count");
    }
    std::vector<VkImage> swapchainImages;
    swapchainImages.resize(swapchainImageCount);
    if (vkGetSwapchainImagesKHR(vkContext.device, m_swapchain.vulkanHandle, &swapchainImageCount, swapchainImages.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to query swapchain images");
    }

    m_swapchain.imageHandles.clear();
    for (const auto vulkanImage : swapchainImages) {
        Image image;
        image.isSwapchainImage = true;
        image.vulkanHandle = vulkanImage;
        image.desc.width = width;
        image.desc.height = height;
        image.desc.depth = 1;
        image.format = m_swapchain.surfaceFormat.format;
        image.desc.type = ImageType::Type2D;
        image.viewPerMip.push_back(createImageView(image, 0, 1));
        image.layoutPerMip.push_back(VK_IMAGE_LAYOUT_UNDEFINED);

        if (m_freeImageHandles.size() > 0) {
            ImageHandle handle = m_freeImageHandles.back();
            m_freeImageHandles.pop_back();
            m_swapchain.imageHandles.push_back(handle);
            m_images[handle.index] = image;
        }
        else {
            m_swapchain.imageHandles.push_back({ (uint32_t)m_images.size() });
            m_images.push_back(image);
        }
    }
}

void RenderBackend::presentImage(const VkSemaphore waitSemaphore) {

    VkPresentInfoKHR present = {};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pNext = nullptr;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &waitSemaphore;
    present.swapchainCount = 1;
    present.pSwapchains = &m_swapchain.vulkanHandle;
    present.pImageIndices = &m_swapchainInputImageIndex;

    VkResult presentResult = VK_SUCCESS;
    present.pResults = &presentResult;

    auto result = vkQueuePresentKHR(vkContext.presentQueue, &present);
    checkVulkanResult(result);
    checkVulkanResult(presentResult);
}

void RenderBackend::setupImgui(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    const ImageFormat swapchainFormat = vulkanImageFormatToImageFormat(m_swapchain.surfaceFormat.format);
    const auto colorAttachment = Attachment(swapchainFormat, AttachmentLoadOp::Load);

    m_ui.renderPass = createVulkanRenderPass(std::vector<Attachment> {colorAttachment});
    createImguiDescriptorPool();

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = vkContext.vulkanInstance;
    init_info.PhysicalDevice = vkContext.physicalDevice;
    init_info.Device = vkContext.device;
    init_info.QueueFamily = vkContext.queueFamilies.graphicsQueueIndex;
    init_info.Queue = vkContext.graphicQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_imguiDescriptorPool;
    init_info.Allocator = nullptr;
    init_info.MinImageCount = m_swapchain.minImageCount;
    init_info.ImageCount = (uint32_t)m_swapchain.imageHandles.size();
    init_info.CheckVkResultFn = nullptr;
    if (!ImGui_ImplVulkan_Init(&init_info, m_ui.renderPass)) {
        throw("ImGui inizialisation error");
    }

    //build fonts texture    
    const auto currentCommandBuffer = m_commandBuffers[0];

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    auto res = vkBeginCommandBuffer(currentCommandBuffer, &begin_info);
    assert(res == VK_SUCCESS);

    ImGui_ImplVulkan_CreateFontsTexture(currentCommandBuffer);

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &currentCommandBuffer;
    res = vkEndCommandBuffer(currentCommandBuffer);
    assert(res == VK_SUCCESS);
    res = vkQueueSubmit(vkContext.graphicQueue, 1, &end_info, VK_NULL_HANDLE);
    assert(res == VK_SUCCESS);

    res = vkDeviceWaitIdle(vkContext.device);
    assert(res == VK_SUCCESS);
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    m_ui.framebuffers = createImGuiFramebuffers();

    const int width = m_images[m_swapchain.imageHandles[0].index].desc.width;
    const int height = m_images[m_swapchain.imageHandles[0].index].desc.height;
    m_ui.passBeginInfos = createImGuiPassBeginInfo(width, height);
}

std::vector<VkFramebuffer> RenderBackend::createImGuiFramebuffers() {
    std::vector<VkFramebuffer> buffers;
    for (const auto& imageHandle : m_swapchain.imageHandles) {
        RenderTarget uiTarget;
        uiTarget.image = imageHandle;
        uiTarget.mipLevel = 0;
        const VkFramebuffer framebuffer = createVulkanFramebuffer({ uiTarget }, m_ui.renderPass);
        buffers.push_back(framebuffer);
    }
    return buffers;
}

std::vector<VkRenderPassBeginInfo> RenderBackend::createImGuiPassBeginInfo(const int width, const int height) {
    std::vector<VkRenderPassBeginInfo> passBeginInfos;
    for (const auto& framebuffer : m_ui.framebuffers) {
        const auto beginInfo = createBeginInfo(width, height, m_ui.renderPass, framebuffer, {});
        passBeginInfos.push_back(beginInfo);
    }
    return passBeginInfos;
}

Image& RenderBackend::getImageRef(const ImageHandle handle) {
    const ImageHandleDecoded decoded = decodeImageHandle(handle);
    if (decoded.isTempImage) {
        const int allocationIndex = m_temporaryImages[decoded.index].allocationIndex;
        assert(allocationIndex >= allocationIndex);
        assert(allocationIndex < m_allocatedTempImages.size());
        return m_allocatedTempImages[allocationIndex].image;
    }
    else {
        return m_images[decoded.index];
    }
}

bool imageDescriptionsMatch(const ImageDescription& d1, const ImageDescription& d2) {
    return
        d1.format       == d2.format    &&
        d1.width        == d2.width     &&
        d1.height       == d2.height    &&
        d1.depth        == d2.depth     &&
        d1.mipCount     == d2.mipCount  &&
        d1.type         == d2.type      &&
        d1.usageFlags   == d2.usageFlags;
}

void RenderBackend::mapOverRenderpassTempImages(std::function<void(const int renderpassImage, 
    const int tempImageIndex)> function) {

    for (int i = 0; i < m_renderPassExecutions.size(); i++) {
        const auto& executionEntry = m_renderPassExecutions[i];
        const RenderPassExecution& execution = getGenericRenderpassInfoFromExecutionEntry(executionEntry,
            m_graphicPassExecutions, m_computePassExecutions);

        for (const auto& imageResource : execution.resources.sampledImages) {
            const auto decodedImageHandle = decodeImageHandle(imageResource.image);
            if (decodedImageHandle.isTempImage) {
                function(i, decodedImageHandle.index);
            }
        }

        for (const auto& imageResource : execution.resources.storageImages) {
            const auto decodedImageHandle = decodeImageHandle(imageResource.image);
            if (decodedImageHandle.isTempImage) {
                function(i, decodedImageHandle.index);
            }
        }
        if (executionEntry.type == RenderPassType::Graphic) {
            const auto& graphicPass = m_graphicPassExecutions[executionEntry.index];
            for (const auto& target : graphicPass.targets) {
                const auto decodedImageHandle = decodeImageHandle(target.image);
                if (decodedImageHandle.isTempImage) {
                    function(i, decodedImageHandle.index);
                }
            }
        }
    }
}

void RenderBackend::allocateTemporaryImages() {

    //compute temp image usage
    struct TempImageUsage {
        int firstUse = std::numeric_limits<int>::max();
        int lastUse = 0;
    };
    std::vector<TempImageUsage> imagesUsage(m_temporaryImages.size());

    std::function<void(const int, const int)> usageLambda = [&imagesUsage](const int renderpassIndex, const int tempImageIndex) {
        TempImageUsage& usage = imagesUsage[tempImageIndex];
        usage.firstUse = std::min(usage.firstUse, renderpassIndex);
        usage.lastUse = std::max(usage.lastUse, renderpassIndex);
    };

    mapOverRenderpassTempImages(usageLambda);

    std::vector<int> allocatedImageLatestUsedPass(m_allocatedTempImages.size(), 0);

    std::function<void(const int, const int)> allocationLambda =
        [&imagesUsage, &allocatedImageLatestUsedPass, this](const int renderPassIndex, const int tempImageIndex) {

        auto& tempImage = m_temporaryImages[tempImageIndex];

        const bool isAlreadyAllocated = tempImage.allocationIndex >= 0;
        if (isAlreadyAllocated) {
            return;
        }

        bool foundAllocatedImage = false;
        const TempImageUsage& usage = imagesUsage[tempImageIndex];

        for (int allocatedImageIndex = 0; allocatedImageIndex < m_allocatedTempImages.size(); allocatedImageIndex++) {

            int& allocatedImageLastUse = allocatedImageLatestUsedPass[allocatedImageIndex];
            auto& allocatedImage = m_allocatedTempImages[allocatedImageIndex];
            const bool allocatedImageAvailable = allocatedImageLastUse < renderPassIndex;

            const bool requirementsMatching = imageDescriptionsMatch(tempImage.desc, allocatedImage.image.desc);
            if (allocatedImageAvailable && requirementsMatching) {
                tempImage.allocationIndex = allocatedImageIndex;
                allocatedImageLastUse = usage.lastUse;
                foundAllocatedImage = true;
                allocatedImage.usedThisFrame = true;
                break;
            }
        }
        if (!foundAllocatedImage) {
            std::cout << "Allocated temp image\n";
            AllocatedTempImage allocatedImage;
            allocatedImage.image = createImageInternal(tempImage.desc, nullptr, 0);
            allocatedImage.usedThisFrame = true;
            tempImage.allocationIndex = m_allocatedTempImages.size();
            m_allocatedTempImages.push_back(allocatedImage);
            allocatedImageLatestUsedPass.push_back(usage.lastUse);
        }
    };
    mapOverRenderpassTempImages(allocationLambda);
}

void RenderBackend::resetAllocatedTempImages() {
    for (int i = 0; i < m_allocatedTempImages.size(); i++) {
        if (!m_allocatedTempImages[i].usedThisFrame) {
            //delete unused image
            std::swap(m_allocatedTempImages.back(), m_allocatedTempImages[i]);
            vkDeviceWaitIdle(vkContext.device); //FIXME: don't use wait idle, use deferred destruction queue instead
            destroyImageInternal(m_allocatedTempImages.back().image);
            m_allocatedTempImages.pop_back();
            std::cout << "Deleted unused image\n";
        }
        else {
            //reset usage
            m_allocatedTempImages[i].usedThisFrame = false;
        }
    }
}

void RenderBackend::updateRenderPassDescriptorSets() {
    for (const auto& executionEntry : m_renderPassExecutions) {
        if (executionEntry.type == RenderPassType::Graphic) {
            const auto& execution = m_graphicPassExecutions[executionEntry.index];
            const VkDescriptorSet descriptorSet = m_renderPasses.getGraphicPassRefByHandle(execution.genericInfo.handle).descriptorSets[m_frameIndexMod2];
            updateDescriptorSet(descriptorSet, execution.genericInfo.resources);
        }
        else {
            const auto& execution = m_computePassExecutions[executionEntry.index];
            const VkDescriptorSet descriptorSet = m_renderPasses.getComputePassRefByHandle(execution.genericInfo.handle).descriptorSets[m_frameIndexMod2];
            updateDescriptorSet(descriptorSet, execution.genericInfo.resources);
        }
    }
}

void RenderBackend::startGraphicPassRecording() {
    for (int i = 0; i < m_graphicPassExecutions.size(); i++) {
        const GraphicPassExecution execution = m_graphicPassExecutions[i];
        const RenderPassHandle passHandle = execution.genericInfo.handle;
        const GraphicPass pass = m_renderPasses.getGraphicPassRefByHandle(passHandle);

        VkCommandBufferInheritanceInfo inheritanceInfo;
        inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        inheritanceInfo.pNext = nullptr;
        inheritanceInfo.renderPass = pass.vulkanRenderPass;
        inheritanceInfo.subpass = 0;
        inheritanceInfo.framebuffer = m_transientFramebuffers[m_frameIndexMod2][i];
        inheritanceInfo.occlusionQueryEnable = false;
        inheritanceInfo.queryFlags = 0;
        inheritanceInfo.pipelineStatistics = 0;

        VkCommandBufferBeginInfo cmdBeginInfo;
        cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBeginInfo.pNext = nullptr;
        cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        cmdBeginInfo.pInheritanceInfo = &inheritanceInfo;

        const int poolCount = m_drawcallCommandPools.size();
        for (int cmdBufferIndex = 0; cmdBufferIndex < poolCount; cmdBufferIndex++) {
            const int finalIndex = cmdBufferIndex + m_frameIndexMod2 * poolCount;
            const VkCommandBuffer meshCommandBuffer = pass.meshCommandBuffers[finalIndex];
            const auto res = vkResetCommandBuffer(meshCommandBuffer, 0);
            assert(res == VK_SUCCESS);

            //reset command buffer
            vkBeginCommandBuffer(meshCommandBuffer, &cmdBeginInfo);

            //prepare for drawcall recording
            vkCmdBindPipeline(meshCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipeline);

            const glm::ivec2 resolution = getResolutionFromRenderTargets(execution.targets);

            //set viewport
            {
                VkViewport viewport;
                viewport.x = 0;
                viewport.y = 0;
                viewport.width = (float)resolution.x;
                viewport.height = (float)resolution.y;
                viewport.minDepth = 0.f;
                viewport.maxDepth = 1.f;

                vkCmdSetViewport(meshCommandBuffer, 0, 1, &viewport);
            }
            //set scissor
            {
                VkRect2D scissor;
                scissor.offset = { 0, 0 };
                scissor.extent.width = resolution.x;
                scissor.extent.height = resolution.y;

                vkCmdSetScissor(meshCommandBuffer, 0, 1, &scissor);
            }
        }
    }
}

Image RenderBackend::createImageInternal(const ImageDescription& desc, const void* initialData, const size_t initialDataSize) {
    const VkFormat format = imageFormatToVulkanFormat(desc.format);
    const uint32_t mipCount = computeImageMipCount(desc);
    const bool bFillImageWithData = initialDataSize > 0;

    Image image;
    image.desc = desc;
    image.format = format;
    image.layoutPerMip = createInitialImageLayouts(mipCount);
    image.vulkanHandle = createVulkanImage(desc, bFillImageWithData);
    image.memory = allocateImageMemory(image.vulkanHandle);
    image.viewPerMip = createImageViews(image, mipCount);

    if (bFillImageWithData) {
        transferDataIntoImage(image, initialData, initialDataSize);
    }
    if (desc.autoCreateMips) {
        generateMipChain(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // most textures with sampled usage are used by the material system
    // the material systems assumes the read_only_optimal layout
    // if no mips are generated the layout will still be transfer_dst or undefined
    // to avoid issues all sampled images without mip generation are manually transitioned to read_only_optimal
    const bool manualLayoutTransitionRequired = bool(desc.usageFlags & ImageUsageFlags::Sampled) && !desc.autoCreateMips;
    if (manualLayoutTransitionRequired) {
        manualImageLayoutTransition(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    const bool imageCanBeSampled = bool(desc.usageFlags & ImageUsageFlags::Sampled);
    if (imageCanBeSampled) {
        addImageToGlobalDescriptorSetLayout(image);
    }
    return image;
}

void RenderBackend::manualImageLayoutTransition(Image& image, const VkImageLayout newLayout) {

    const auto transitionCmdBuffer = beginOneTimeUseCommandBuffer();
    const auto newLayoutBarriers = createImageBarriers(image, newLayout,
        VK_ACCESS_TRANSFER_WRITE_BIT, 0, (uint32_t)image.viewPerMip.size());
    barriersCommand(transitionCmdBuffer, newLayoutBarriers, std::vector<VkBufferMemoryBarrier> {});

    auto res = vkEndCommandBuffer(transitionCmdBuffer);
    checkVulkanResult(res);

    VkFence fence = submitOneTimeUseCmdBuffer(transitionCmdBuffer, vkContext.transferQueue);

    res = vkWaitForFences(vkContext.device, 1, &fence, VK_TRUE, UINT64_MAX);
    checkVulkanResult(res);

    vkDestroyFence(vkContext.device, fence, nullptr);
    vkFreeCommandBuffers(vkContext.device, m_transientCommandPool, 1, &transitionCmdBuffer);
}

void RenderBackend::addImageToGlobalDescriptorSetLayout(Image& image) {
    const bool isFreeIndexAvailable = m_globalTextureArrayDescriptorSetFreeTextureIndices.size() > 0;
    if (isFreeIndexAvailable) {
        image.globalDescriptorSetIndex = m_globalTextureArrayDescriptorSetFreeTextureIndices.back();
        m_globalTextureArrayDescriptorSetFreeTextureIndices.pop_back();
    }
    else {
        image.globalDescriptorSetIndex = (int32_t)m_globalTextureArrayDescriptorSetTextureCount;
        m_globalTextureArrayDescriptorSetTextureCount++;
    }
    setGlobalTextureArrayDescriptorSetTexture(image.viewPerMip[0], image.globalDescriptorSetIndex);
}

VulkanAllocation RenderBackend::allocateImageMemory(const VkImage image) {
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(vkContext.device, image, &memoryRequirements);

    const VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VulkanAllocation allocation;
    if (!m_vkAllocator.allocate(memoryRequirements, memoryFlags, &allocation)) {
        throw("Could not allocate image memory");
    }

    auto res = vkBindImageMemory(vkContext.device, image, allocation.vkMemory, allocation.offset);
    checkVulkanResult(res);

    return allocation;
}

Buffer RenderBackend::createBufferInternal(const VkDeviceSize size, const std::vector<uint32_t>& queueFamilies, const VkBufferUsageFlags usage, const uint32_t memoryFlags) {

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.flags = 0;
    bufferInfo.size = size;
    bufferInfo.usage = usage;

    //find unique queue families    
    std::vector<uint32_t> uniqueQueueFamilies;
    for (const auto& index : queueFamilies) {
        if (!vectorContains(uniqueQueueFamilies, index)) {
            uniqueQueueFamilies.push_back(index);
        }
    }

    if (queueFamilies.size() > 1) {
        bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
    }
    else {
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    bufferInfo.queueFamilyIndexCount = (uint32_t)uniqueQueueFamilies.size();
    bufferInfo.pQueueFamilyIndices = uniqueQueueFamilies.data();

    Buffer buffer;
    buffer.size = size;
    auto res = vkCreateBuffer(vkContext.device, &bufferInfo, nullptr, &buffer.vulkanHandle);
    assert(res == VK_SUCCESS);

    //memory
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(vkContext.device, buffer.vulkanHandle, &memoryRequirements);
    if (!m_vkAllocator.allocate(memoryRequirements, memoryFlags, &buffer.memory)) {
        throw("Could not allocate buffer memory");
    }
    res = vkBindBufferMemory(vkContext.device, buffer.vulkanHandle, buffer.memory.vkMemory, buffer.memory.offset);
    assert(res == VK_SUCCESS);

    return buffer;
}

VkImageSubresourceLayers RenderBackend::createSubresourceLayers(const Image& image, const uint32_t mipLevel) {
    VkImageSubresourceLayers layers;
    layers.aspectMask = isVulkanDepthFormat(image.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    layers.mipLevel = mipLevel;
    layers.baseArrayLayer = 0;
    layers.layerCount = 1;
    return layers;
}

void RenderBackend::transferDataIntoImage(Image& target, const void* data, const VkDeviceSize size) {

    //BCn compressed formats have certain properties that have to be considered when copying data
    bool isBCnCompressed = getImageFormatIsBCnCompressed(target.desc.format);

    //use double to avoid 4 to 8 byte casting warnings after arithmetic
    double bytePerPixel = getImageFormatBytePerPixel(target.desc.format);

    //if size is bigger than mip level 0 automatically switch to next mip level
    uint32_t mipLevel = 0;
    VkDeviceSize currentMipWidth = target.desc.width;
    VkDeviceSize currentMipHeight = target.desc.height;
    VkDeviceSize currentMipDepth = target.desc.depth;

    VkDeviceSize bytesPerRow    = (size_t)(target.desc.width * bytePerPixel);
    VkDeviceSize currentMipSize = (VkDeviceSize)(currentMipWidth * currentMipHeight * currentMipDepth * bytePerPixel);

    //memory offset per mip is tracked separately to check if a mip border is reached
    VkDeviceSize mipMemoryOffset = 0;

    //total offset is used to check if entire data has been copied
    VkDeviceSize totalMemoryOffset = 0;

    //if the image data is bigger than the staging buffer multiple copies are needed
    //use a while loop because currentMemoryOffset is increased by copySize, which can vary at mip borders    
    //TODO: creation of cmd buffer and fence in loop is inefficient
    while (totalMemoryOffset < size) {

        //check if mip border is reached
        if (mipMemoryOffset >= currentMipSize) {
            mipLevel++;
            //resoltion is halved at every mip level
            currentMipWidth /= 2;
            currentMipHeight /= 2;
            currentMipDepth /= 2;
            currentMipDepth = glm::max(currentMipDepth, VkDeviceSize(1));
            bytesPerRow /= 2;

            // reduce size depending on dimensions
            if (target.desc.type == ImageType::Type1D) {
                currentMipSize /= 2;
            }
            else if (target.desc.type == ImageType::Type2D) {
                currentMipSize /= 4;
            }
            else if (target.desc.type == ImageType::Type3D) {
                currentMipSize /= 8;
            }
            else {
                std::cout << "Error: unknown image type" << std::endl;
                assert(false);
                return;
            }

            // memory offset per mip is reset
            mipMemoryOffset = 0;

            // BCn compressed textures store at least a 4x4 pixel block, resulting in at least a 4 pixel row
            if (isBCnCompressed) {
                const VkDeviceSize minPixelPerRox = 4;
                bytesPerRow = std::max(bytesPerRow, VkDeviceSize(minPixelPerRox * bytePerPixel));

                const VkDeviceSize minPixelsPerMip = 16;
                currentMipSize = std::max(currentMipSize, VkDeviceSize(minPixelsPerMip * bytePerPixel));
            }
        }

        
        // the size to copy is limited either by
        // -the staging buffer size
        // -the size left to copy on the current mip level
        VkDeviceSize copySize = std::min(m_stagingBufferSize, currentMipSize - mipMemoryOffset);

        // always copy entire rows
        copySize = copySize / bytesPerRow * bytesPerRow;

        // copy data to staging buffer
        fillHostVisibleCoherentBuffer(m_stagingBuffer, (char*)data + totalMemoryOffset, copySize);

        // begin command buffer for copying
        VkCommandBuffer copyBuffer = beginOneTimeUseCommandBuffer();

        // layout transition to transfer_dst the first time
        if (totalMemoryOffset == 0) {
            const auto toTransferDstBarrier = createImageBarriers(target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_TRANSFER_READ_BIT, 0, (uint32_t)target.viewPerMip.size());

            barriersCommand(copyBuffer, toTransferDstBarrier, std::vector<VkBufferMemoryBarrier> {});
        }
        

        // calculate which region to copy
        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.imageSubresource = createSubresourceLayers(target, mipLevel);
        // entire rows are copied, so the starting row(offset.y) is the current mip offset divided by the row size
        region.imageOffset = { 0, int32_t(mipMemoryOffset / bytesPerRow), 0 };
        region.bufferRowLength = (uint32_t)currentMipWidth;
        region.bufferImageHeight = (uint32_t)currentMipHeight;
        // copy as many rows as fit into the copy size, without going over the mip height
        region.imageExtent.height = (uint32_t)std::min(copySize / bytesPerRow, currentMipHeight);
        region.imageExtent.width = (uint32_t)currentMipWidth;
        region.imageExtent.depth = (uint32_t)currentMipDepth;

        // BCn compressed textures are stored in 4x4 pixel blocks, so that is the minimum buffer size
        if (isBCnCompressed) {
            region.bufferRowLength      = std::max(region.bufferRowLength,      (uint32_t)4);;
            region.bufferImageHeight    = std::max(region.bufferImageHeight,    (uint32_t)4);;
        }

        // issue for commands, then wait
        vkCmdCopyBufferToImage(copyBuffer, m_stagingBuffer.vulkanHandle, target.vulkanHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        vkEndCommandBuffer(copyBuffer);
        VkFence fence = submitOneTimeUseCmdBuffer(copyBuffer, vkContext.transferQueue);
        auto result = vkWaitForFences(vkContext.device, 1, &fence, VK_TRUE, UINT64_MAX);
        checkVulkanResult(result);

        // cleanup
        vkDestroyFence(vkContext.device, fence, nullptr);
        vkFreeCommandBuffers(vkContext.device, m_transientCommandPool, 1, &copyBuffer);

        // update memory offsets
        // BCn compressed textures store at least a 4x4 pixel block
        if (isBCnCompressed) {
            mipMemoryOffset     += std::max(copySize, 4 * 4 * (VkDeviceSize)bytePerPixel);
            totalMemoryOffset   += std::max(copySize, 4 * 4 * (VkDeviceSize)bytePerPixel);
        }
        else {
            mipMemoryOffset     += copySize;
            totalMemoryOffset   += copySize;
        }
    }
}

void RenderBackend::generateMipChain(Image& image, const VkImageLayout newLayout) {

    //check for linear filtering support
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(vkContext.physicalDevice, image.format, &formatProps);
    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("physical device lacks linear filtering support for used format");
    }

    VkImageBlit blitInfo;

    //offset base stays zero
    blitInfo.srcOffsets[0].x = 0;
    blitInfo.srcOffsets[0].y = 0;
    blitInfo.srcOffsets[0].z = 0;

    blitInfo.dstOffsets[0].x = 0;
    blitInfo.dstOffsets[0].y = 0;
    blitInfo.dstOffsets[0].z = 0;

    //initial offset extent
    blitInfo.srcOffsets[1].x = image.desc.width;
    blitInfo.srcOffsets[1].y = image.desc.height;
    blitInfo.srcOffsets[1].z = image.desc.depth;

    blitInfo.dstOffsets[1].x = blitInfo.srcOffsets[1].x != 1 ? blitInfo.srcOffsets[1].x / 2 : 1;
    blitInfo.dstOffsets[1].y = blitInfo.srcOffsets[1].y != 1 ? blitInfo.srcOffsets[1].y / 2 : 1;
    blitInfo.dstOffsets[1].z = blitInfo.srcOffsets[1].z != 1 ? blitInfo.srcOffsets[1].z / 2 : 1;

    const auto blitCmdBuffer = beginOneTimeUseCommandBuffer();

    for (uint32_t srcMip = 0; srcMip < image.viewPerMip.size() - 1; srcMip++) {

        //barriers
        auto barriers = createImageBarriers(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, srcMip, 1); //src
        const auto dstBarriers = createImageBarriers(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, srcMip + 1, 1); //dst
        barriers.insert(barriers.end(), dstBarriers.begin(), dstBarriers.end());

        barriersCommand(blitCmdBuffer, barriers, std::vector<VkBufferMemoryBarrier> {});

        //blit operation
        blitInfo.srcSubresource = createSubresourceLayers(image, srcMip );
        blitInfo.dstSubresource = createSubresourceLayers(image, srcMip + 1);

        vkCmdBlitImage(blitCmdBuffer, image.vulkanHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image.vulkanHandle, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitInfo, VK_FILTER_LINEAR);

        //update offsets
        blitInfo.srcOffsets[1].x /= blitInfo.srcOffsets[1].x != 1 ? 2 : 1;
        blitInfo.srcOffsets[1].y /= blitInfo.srcOffsets[1].y != 1 ? 2 : 1;
        blitInfo.srcOffsets[1].z /= blitInfo.srcOffsets[1].z != 1 ? 2 : 1;

        blitInfo.dstOffsets[1].x = blitInfo.srcOffsets[1].x != 1 ? blitInfo.srcOffsets[1].x / 2 : 1;
        blitInfo.dstOffsets[1].y = blitInfo.srcOffsets[1].y != 1 ? blitInfo.srcOffsets[1].y / 2 : 1;
        blitInfo.dstOffsets[1].z = blitInfo.srcOffsets[1].z != 1 ? blitInfo.srcOffsets[1].z / 2 : 1;
    }

    //bring image into new layout
    const auto newLayoutBarriers = createImageBarriers(image, newLayout, VK_ACCESS_TRANSFER_WRITE_BIT, 0, (uint32_t)image.viewPerMip.size());
    barriersCommand(blitCmdBuffer, newLayoutBarriers, std::vector<VkBufferMemoryBarrier> {});

    //end recording
    auto res = vkEndCommandBuffer(blitCmdBuffer);
    assert(res == VK_SUCCESS);

    //submit
    VkFence fence = submitOneTimeUseCmdBuffer(blitCmdBuffer, vkContext.transferQueue);

    res = vkWaitForFences(vkContext.device, 1, &fence, VK_TRUE, UINT64_MAX);
    assert(res == VK_SUCCESS);

    //cleanup
    vkDestroyFence(vkContext.device, fence, nullptr);
    vkFreeCommandBuffers(vkContext.device, m_transientCommandPool, 1, &blitCmdBuffer);
}

void RenderBackend::fillBuffer(Buffer target, const void* data, const VkDeviceSize size) {

    //TODO: creation of cmd buffer and fence in loop is somewhat inefficient
    for (VkDeviceSize currentMemoryOffset = 0; currentMemoryOffset < size; currentMemoryOffset += m_stagingBufferSize) {

        VkDeviceSize copySize = std::min(m_stagingBufferSize, size - currentMemoryOffset);

        //copy data to staging buffer
        void* mappedData;
        auto res = vkMapMemory(vkContext.device, m_stagingBuffer.memory.vkMemory, 0, copySize, 0, (void**)&mappedData);
        assert(res == VK_SUCCESS);
        memcpy(mappedData, (char*)data + currentMemoryOffset, copySize);
        vkUnmapMemory(vkContext.device, m_stagingBuffer.memory.vkMemory);

        //copy staging buffer to dst
        VkCommandBuffer copyCmdBuffer = beginOneTimeUseCommandBuffer();

        //copy command
        VkBufferCopy region = {};
        region.srcOffset = 0;
        region.dstOffset = currentMemoryOffset;
        region.size = copySize;
        vkCmdCopyBuffer(copyCmdBuffer, m_stagingBuffer.vulkanHandle, target.vulkanHandle, 1, &region);
        res = vkEndCommandBuffer(copyCmdBuffer);       
        assert(res == VK_SUCCESS);

        //submit and wait
        VkFence fence = submitOneTimeUseCmdBuffer(copyCmdBuffer, vkContext.transferQueue);
        res = vkWaitForFences(vkContext.device, 1, &fence, VK_TRUE, UINT64_MAX);
        assert(res == VK_SUCCESS);

        //cleanup
        vkDestroyFence(vkContext.device, fence, nullptr);
        vkFreeCommandBuffers(vkContext.device, m_transientCommandPool, 1, &copyCmdBuffer);
    }
}

void RenderBackend::fillHostVisibleCoherentBuffer(Buffer target, const void* data, const VkDeviceSize size) {
    void* mappedData;
    auto result = vkMapMemory(vkContext.device, target.memory.vkMemory, target.memory.offset, size, 0, (void**)&mappedData);
    checkVulkanResult(result);
    memcpy(mappedData, data, size);
    vkUnmapMemory(vkContext.device, m_stagingBuffer.memory.vkMemory);
}

VkCommandPool RenderBackend::createCommandPool(const uint32_t queueFamilyIndex, const VkCommandPoolCreateFlagBits flags) {

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = flags;
    poolInfo.queueFamilyIndex = queueFamilyIndex;

    VkCommandPool pool;
    auto res = vkCreateCommandPool(vkContext.device, &poolInfo, nullptr, &pool);
    checkVulkanResult(res);

    return pool;
}

VkCommandBuffer RenderBackend::allocateCommandBuffer(const VkCommandBufferLevel level, const VkCommandPool& pool) {

    VkCommandBufferAllocateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.commandPool = pool;
    bufferInfo.level = level;
    bufferInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    auto res = vkAllocateCommandBuffers(vkContext.device, &bufferInfo, &commandBuffer);
    checkVulkanResult(res);

    return commandBuffer;
}

VkCommandBuffer RenderBackend::beginOneTimeUseCommandBuffer() {

    //allocate copy command buffer
    VkCommandBufferAllocateInfo command = {};
    command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command.pNext = nullptr;
    command.commandPool = m_transientCommandPool;
    command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    auto res = vkAllocateCommandBuffers(vkContext.device, &command, &cmdBuffer);
    assert(res == VK_SUCCESS);

    //begin recording
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = VK_NULL_HANDLE;

    res = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    assert(res == VK_SUCCESS);

    return cmdBuffer;
}

VkFence RenderBackend::submitOneTimeUseCmdBuffer(VkCommandBuffer cmdBuffer, VkQueue queue) {
    //submit commands
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = nullptr;
    submit.waitSemaphoreCount = 0;
    submit.pWaitSemaphores = nullptr;
    submit.pWaitDstStageMask = nullptr;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmdBuffer;
    submit.signalSemaphoreCount = 0;
    submit.pSignalSemaphores = nullptr;

    VkFence fence = createFence();
    auto res = vkResetFences(vkContext.device, 1, &fence);
    assert(res == VK_SUCCESS);

    res = vkQueueSubmit(queue, 1, &submit, fence);
    assert(res == VK_SUCCESS);

    return fence;
}

void RenderBackend::startDebugLabel(const VkCommandBuffer cmdBuffer, const std::string& name) {
    const VkDebugUtilsLabelEXT uiLabel =
    {
        VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        nullptr,
        name.c_str(),
        { 1.0f, 1.0f, 1.0f, 1.0f },
    };
    m_debugExtFunctions.vkCmdBeginDebugUtilsLabelEXT(cmdBuffer, &uiLabel);
}

void RenderBackend::endDebugLabel(const VkCommandBuffer cmdBuffer) {
    m_debugExtFunctions.vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
}

void RenderBackend::createImguiDescriptorPool() {
    //taken from imgui vulkan example, could not find any info if imgui can work with less allocations
    VkDescriptorPoolSize pool_sizes[] =
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
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    auto res = vkCreateDescriptorPool(vkContext.device, &pool_info, nullptr, &m_imguiDescriptorPool);
    checkVulkanResult(res);
}

void RenderBackend::createDescriptorPool() {

    const auto& initialSizes = m_descriptorPoolInitialAllocationSizes;

    const uint32_t typeCount = 5;

    VkDescriptorPoolCreateInfo poolInfo;
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = 0;
    poolInfo.maxSets = m_descriptorPoolInitialAllocationSizes.setCount;
    poolInfo.poolSizeCount = typeCount;

    VkDescriptorPoolSize poolSize[typeCount];
    poolSize[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSize[0].descriptorCount = initialSizes.imageSampled;

    poolSize[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize[1].descriptorCount = initialSizes.imageStorage;

    poolSize[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize[2].descriptorCount = initialSizes.uniformBuffer;

    poolSize[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize[3].descriptorCount = initialSizes.storageBuffer;

    poolSize[4].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSize[4].descriptorCount = initialSizes.sampler;

    poolInfo.pPoolSizes = poolSize;

    DescriptorPool pool;
    pool.freeAllocations = initialSizes;
    auto res = vkCreateDescriptorPool(vkContext.device, &poolInfo, nullptr, &pool.vkPool);
    checkVulkanResult(res);
    
    m_descriptorPools.push_back(pool);
}

DescriptorPoolAllocationSizes RenderBackend::descriptorSetAllocationSizeFromShaderLayout(const ShaderLayout& layout) {
    DescriptorPoolAllocationSizes sizes;
    sizes.setCount = 1;
    sizes.imageSampled  = (uint32_t)layout.sampledImageBindings.size();
    sizes.imageStorage  = (uint32_t)layout.storageImageBindings.size();
    sizes.storageBuffer = (uint32_t)layout.storageBufferBindings.size();
    sizes.uniformBuffer = (uint32_t)layout.uniformBufferBindings.size();
    sizes.sampler       = (uint32_t)layout.samplerBindings.size();
    return sizes;
}

VkDescriptorSet RenderBackend::allocateDescriptorSet(const VkDescriptorSetLayout setLayout, const DescriptorPoolAllocationSizes& requiredSizes) {

    VkDescriptorSetAllocateInfo setInfo;
    setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setInfo.pNext = nullptr;
    setInfo.descriptorPool = VK_NULL_HANDLE;
    setInfo.descriptorSetCount = 1;
    setInfo.pSetLayouts = &setLayout;

    //find descriptor pool with enough free allocations
    const auto hasPoolEnoughFreeAllocations = [](const DescriptorPool& pool, const DescriptorPoolAllocationSizes& requiredSizes) {
        return
            pool.freeAllocations.setCount       >= requiredSizes.setCount &&
            pool.freeAllocations.imageSampled   >= requiredSizes.imageSampled &&
            pool.freeAllocations.imageStorage   >= requiredSizes.imageStorage &&
            pool.freeAllocations.storageBuffer  >= requiredSizes.storageBuffer &&
            pool.freeAllocations.uniformBuffer  >= requiredSizes.uniformBuffer &&
            pool.freeAllocations.sampler        >= requiredSizes.sampler;
    };

    //returns size of first - second
    const auto subtractDescriptorPoolSizes = [](const DescriptorPoolAllocationSizes & first, const DescriptorPoolAllocationSizes & second) {
        DescriptorPoolAllocationSizes result;
        result.setCount         = first.setCount        - second.setCount;
        result.imageSampled     = first.imageSampled    - second.imageSampled;
        result.imageStorage     = first.imageStorage    - second.imageStorage;
        result.storageBuffer    = first.storageBuffer   - second.storageBuffer;
        result.uniformBuffer    = first.uniformBuffer   - second.uniformBuffer;
        result.sampler          = first.sampler         - second.sampler;
        return result;
    };

    for (auto& pool : m_descriptorPools) {
        if (hasPoolEnoughFreeAllocations(pool, requiredSizes)) {
            setInfo.descriptorPool = pool.vkPool;
            pool.freeAllocations = subtractDescriptorPoolSizes(pool.freeAllocations, requiredSizes);
        }
    }

    //if none has been found allocate a new pool
    if (setInfo.descriptorPool == VK_NULL_HANDLE) {
        createDescriptorPool();
        setInfo.descriptorPool = m_descriptorPools.back().vkPool;
    }

    VkDescriptorSet descriptorSet;
    auto result = vkAllocateDescriptorSets(vkContext.device, &setInfo, &descriptorSet);
    checkVulkanResult(result);

    return descriptorSet;
}

void RenderBackend::updateDescriptorSet(const VkDescriptorSet set, const RenderPassResources& resources) {

    auto createWriteDescriptorSet = [set](const uint32_t binding, const VkDescriptorType type, const VkDescriptorBufferInfo* bufferInfo, const VkDescriptorImageInfo* imageInfo) {
        VkWriteDescriptorSet writeSet;
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.pNext = nullptr;
        writeSet.dstSet = set;
        writeSet.dstBinding = binding;
        writeSet.dstArrayElement = 0;
        writeSet.descriptorCount = 1;
        writeSet.descriptorType = type;
        writeSet.pImageInfo = imageInfo;
        writeSet.pBufferInfo = bufferInfo;
        writeSet.pTexelBufferView = nullptr;
        return writeSet;
    };

    std::vector<VkWriteDescriptorSet> descriptorInfos;

    //buffer and images are given via pointer
    //stored in vector to keep pointer valid
    //resize first to avoid push_back invalidating pointers
    uint32_t imageInfoIndex = 0;
    uint32_t bufferInfoIndex = 0;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;

    imageInfos.resize(resources.samplers.size() + resources.storageImages.size() + resources.sampledImages.size());
    bufferInfos.resize(resources.uniformBuffers.size() + resources.storageBuffers.size());

    //samplers
    for (const auto& resource : resources.samplers) {
        VkDescriptorImageInfo samplerInfo;
        samplerInfo.sampler = m_samplers[resource.sampler.index];
        imageInfos[imageInfoIndex] = samplerInfo;
        const auto writeSet = createWriteDescriptorSet(resource.binding, VK_DESCRIPTOR_TYPE_SAMPLER, nullptr, &imageInfos[imageInfoIndex]);
        imageInfoIndex++;
        descriptorInfos.push_back(writeSet);
    }

    //sampled images
    for (const auto& resource : resources.sampledImages) {
        VkDescriptorImageInfo imageInfo;

        bool isUsedAsStorageImage = false;
        for (const auto storageResources : resources.storageImages) {
            if (storageResources.image.index == resource.image.index) {
                isUsedAsStorageImage = true;
            }
        }

        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        if (isUsedAsStorageImage) {
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }

        imageInfo.imageView = getImageRef(resource.image).viewPerMip[resource.mipLevel];
        imageInfos[imageInfoIndex] = imageInfo;
        const auto writeSet = createWriteDescriptorSet(resource.binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 
            nullptr, &imageInfos[imageInfoIndex]);
        imageInfoIndex++;
        descriptorInfos.push_back(writeSet);
    }

    //storage images
    for (const auto& resource : resources.storageImages) {
        VkDescriptorImageInfo imageInfo;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = getImageRef(resource.image).viewPerMip[resource.mipLevel];
        imageInfos[imageInfoIndex] = imageInfo;
        const auto writeSet = createWriteDescriptorSet(resource.binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr, &imageInfos[imageInfoIndex]);
        imageInfoIndex++;
        descriptorInfos.push_back(writeSet);
    }

    //uniform buffer
    for (const auto& resource : resources.uniformBuffers) {
        Buffer buffer = m_uniformBuffers[resource.buffer.index];
        VkDescriptorBufferInfo bufferInfo;
        bufferInfo.buffer = buffer.vulkanHandle;
        bufferInfo.offset = 0;
        bufferInfo.range = buffer.size;
        bufferInfos[bufferInfoIndex] = bufferInfo;
        const auto writeSet = createWriteDescriptorSet(resource.binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &bufferInfos[bufferInfoIndex], nullptr);
        bufferInfoIndex++;
        descriptorInfos.push_back(writeSet);
    }

    //storage buffer
    for (const auto& resource : resources.storageBuffers) {
        Buffer buffer = m_storageBuffers[resource.buffer.index];
        VkDescriptorBufferInfo bufferInfo;
        bufferInfo.buffer = buffer.vulkanHandle;
        bufferInfo.offset = 0;
        bufferInfo.range = buffer.size;
        bufferInfos[bufferInfoIndex] = bufferInfo;
        const auto writeSet = createWriteDescriptorSet(resource.binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
            &bufferInfos[bufferInfoIndex], nullptr);
        bufferInfoIndex++;
        descriptorInfos.push_back(writeSet);
    }

    vkUpdateDescriptorSets(vkContext.device, (uint32_t)descriptorInfos.size(), descriptorInfos.data(), 0, nullptr);
}

VkDescriptorSetLayout RenderBackend::createDescriptorSetLayout(const ShaderLayout& shaderLayout) {

    const std::vector<uint32_t>* bindingLists[5] = {
        &shaderLayout.samplerBindings,
        &shaderLayout.sampledImageBindings,
        &shaderLayout.storageImageBindings,
        &shaderLayout.storageBufferBindings,
        &shaderLayout.uniformBufferBindings
    };

    const VkDescriptorType type[5] = {
        VK_DESCRIPTOR_TYPE_SAMPLER,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    };

    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
    for (uint32_t typeIndex = 0; typeIndex < 5; typeIndex++) {
        for (const auto binding : *bindingLists[typeIndex]) {
            VkDescriptorSetLayoutBinding layoutBinding;
            layoutBinding.binding = binding;
            layoutBinding.descriptorType = type[typeIndex];
            layoutBinding.descriptorCount = 1;
            layoutBinding.stageFlags = VK_SHADER_STAGE_ALL;
            layoutBinding.pImmutableSamplers = nullptr;
            layoutBindings.push_back(layoutBinding);
        }
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.flags = 0;
    layoutInfo.bindingCount = (uint32_t)layoutBindings.size();
    layoutInfo.pBindings = layoutBindings.data();

    VkDescriptorSetLayout setLayout;
    auto res = vkCreateDescriptorSetLayout(vkContext.device, &layoutInfo, nullptr, &setLayout);
    checkVulkanResult(res);

    return setLayout;
}

VkPipelineLayout RenderBackend::createPipelineLayout(const VkDescriptorSetLayout setLayout, const size_t pushConstantSize, 
    const VkShaderStageFlags stageFlags) {

    VkDescriptorSetLayout setLayouts[3] = { m_globalDescriptorSetLayout, setLayout, m_globalTextureArrayDescriporSetLayout };
    uint32_t setCount = 3;

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.flags = 0;
    layoutInfo.setLayoutCount = setCount;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pPushConstantRanges = nullptr;

    VkPushConstantRange pushConstantRange;
    if (pushConstantSize > 0) {
        pushConstantRange.stageFlags = stageFlags;
        pushConstantRange.offset = 0;
        pushConstantRange.size = (uint32_t)pushConstantSize;
        layoutInfo.pPushConstantRanges = &pushConstantRange;
        layoutInfo.pushConstantRangeCount = 1;
    }
    
    VkPipelineLayout layout = {};
    auto res = vkCreatePipelineLayout(vkContext.device, &layoutInfo, nullptr, &layout);
    checkVulkanResult(res);

    return layout;
}

ComputePass RenderBackend::createComputePassInternal(const ComputePassDescription& desc, const std::vector<uint32_t>& spirV) {

    ComputePass pass;
    pass.computePassDesc = desc;
    VkComputePipelineCreateInfo pipelineInfo;

    const VkShaderModule module = createShaderModule(spirV);
    const ShaderReflection reflection = performComputeShaderReflection(spirV);
    pass.descriptorSetLayout = createDescriptorSetLayout(reflection.shaderLayout);
    pass.pipelineLayout = createPipelineLayout(pass.descriptorSetLayout, reflection.pushConstantByteSize, VK_SHADER_STAGE_COMPUTE_BIT);
    pass.pushConstantSize = reflection.pushConstantByteSize;

    VulkanShaderCreateAdditionalStructs additionalStructs;

    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.flags = 0;
    pipelineInfo.stage = createPipelineShaderStageInfos(module, VK_SHADER_STAGE_COMPUTE_BIT,
        desc.shaderDescription.specialisationConstants, &additionalStructs);
    pipelineInfo.layout = pass.pipelineLayout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = 0;

    auto res = vkCreateComputePipelines(vkContext.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pass.pipeline);
    checkVulkanResult(res);

    //shader module no needed anymore
    vkDestroyShaderModule(vkContext.device, module, nullptr);

    //descriptor set    
    const auto setSizes = descriptorSetAllocationSizeFromShaderLayout(reflection.shaderLayout);
    pass.descriptorSets[0] = allocateDescriptorSet(pass.descriptorSetLayout, setSizes);
    pass.descriptorSets[1] = allocateDescriptorSet(pass.descriptorSetLayout, setSizes);

    return pass;
}

void RenderBackend::initGlobalTextureArrayDescriptorSetLayout() {

    VkDescriptorSetLayoutBinding textureArrayBinding;
    textureArrayBinding.binding = 0;
    textureArrayBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    textureArrayBinding.descriptorCount = maxTextureCount;
    textureArrayBinding.stageFlags = VK_SHADER_STAGE_ALL;
    textureArrayBinding.pImmutableSamplers = nullptr;

    const VkDescriptorBindingFlags flags = 
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo flagInfo;
    flagInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flagInfo.pNext = nullptr;
    flagInfo.bindingCount = 1;
    flagInfo.pBindingFlags = &flags;

    VkDescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = &flagInfo;
    layoutInfo.flags = 0;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &textureArrayBinding;

    const VkResult result = vkCreateDescriptorSetLayout(vkContext.device, &layoutInfo, nullptr, &m_globalTextureArrayDescriporSetLayout);
    checkVulkanResult(result);
}

void RenderBackend::setGlobalTextureArrayDescriptorSetTexture(const VkImageView imageView, const uint32_t index) {

    VkDescriptorImageInfo imageInfo;
    imageInfo.imageView = imageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write;
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = nullptr;
    write.dstSet = m_globalTextureArrayDescriptorSet;
    write.dstBinding = 0;
    write.dstArrayElement = index;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo = &imageInfo;
    write.pBufferInfo = nullptr;
    write.pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(vkContext.device, 1, &write, 0, nullptr);
}

void RenderBackend::initGlobalTextureArrayDescriptorSet() {
    DescriptorPoolAllocationSizes layoutSizes;
    layoutSizes.imageSampled = maxTextureCount;

    VkDescriptorPoolSize poolSize;
    poolSize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSize.descriptorCount = maxTextureCount;

    VkDescriptorPoolCreateInfo poolInfo;
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = 0;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    VkResult result = vkCreateDescriptorPool(vkContext.device, &poolInfo, nullptr, &m_globalTextureArrayDescriptorPool);
    checkVulkanResult(result);

    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo;
    variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variableDescriptorCountInfo.pNext = nullptr;
    variableDescriptorCountInfo.descriptorSetCount = 1;
    variableDescriptorCountInfo.pDescriptorCounts = &maxTextureCount;

    VkDescriptorSetAllocateInfo setInfo;
    setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setInfo.pNext = &variableDescriptorCountInfo;
    setInfo.descriptorPool = m_globalTextureArrayDescriptorPool;
    setInfo.descriptorSetCount = 1;
    setInfo.pSetLayouts = &m_globalTextureArrayDescriporSetLayout;

    result = vkAllocateDescriptorSets(vkContext.device, &setInfo, &m_globalTextureArrayDescriptorSet);
    checkVulkanResult(result);
}

GraphicPass RenderBackend::createGraphicPassInternal(const GraphicPassDescription& desc, const GraphicPassShaderSpirV& spirV) {

    GraphicPass pass;
    pass.graphicPassDesc = desc;

    for (int frameIndex = 0; frameIndex < 2; frameIndex++) {
        for (int poolIndex = 0; poolIndex < m_drawcallCommandPools.size(); poolIndex++) {
            const VkCommandPool pool = m_drawcallCommandPools[poolIndex];
            pass.meshCommandBuffers.push_back(allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY, pool));
        }
    }

    const VkShaderModule vertexModule   = createShaderModule(spirV.vertex);
    const VkShaderModule fragmentModule = createShaderModule(spirV.fragment);

    VkShaderModule geometryModule = VK_NULL_HANDLE;
    VkShaderModule tesselationControlModule = VK_NULL_HANDLE;
    VkShaderModule tesselationEvaluationModule = VK_NULL_HANDLE;
    if (spirV.geometry.has_value()) {
        geometryModule = createShaderModule(spirV.geometry.value());
    }
    if (desc.shaderDescriptions.tesselationControl.has_value()) {
        assert(desc.shaderDescriptions.tesselationEvaluation.has_value());   //both shaders must be defined or none
        tesselationControlModule    = createShaderModule(spirV.tessellationControl.value());
        tesselationEvaluationModule = createShaderModule(spirV.tessellationEvaluation.value());
    }

    //create module infos    
    std::vector<VkPipelineShaderStageCreateInfo> stages;

    VulkanShaderCreateAdditionalStructs additionalStructs[5];

    stages.push_back(createPipelineShaderStageInfos(vertexModule,   VK_SHADER_STAGE_VERTEX_BIT,   
        desc.shaderDescriptions.vertex.specialisationConstants, &additionalStructs[0]));
    stages.push_back(createPipelineShaderStageInfos(fragmentModule, VK_SHADER_STAGE_FRAGMENT_BIT, 
        desc.shaderDescriptions.fragment.specialisationConstants, &additionalStructs[1]));

    if (geometryModule != VK_NULL_HANDLE) {
        stages.push_back(createPipelineShaderStageInfos(geometryModule, VK_SHADER_STAGE_GEOMETRY_BIT, 
            desc.shaderDescriptions.geometry.value().specialisationConstants, &additionalStructs[2]));
    }
    if (tesselationControlModule != VK_NULL_HANDLE) {
        stages.push_back(createPipelineShaderStageInfos(tesselationControlModule, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, 
            desc.shaderDescriptions.tesselationControl.value().specialisationConstants, &additionalStructs[3]));
        stages.push_back(createPipelineShaderStageInfos(tesselationEvaluationModule, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
            desc.shaderDescriptions.tesselationEvaluation.value().specialisationConstants, &additionalStructs[4]));
    }

    VkShaderStageFlags pipelineLayoutStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    if (desc.shaderDescriptions.geometry.has_value()) {
        pipelineLayoutStageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    }

    const ShaderReflection reflection = performShaderReflection(spirV);
    pass.descriptorSetLayout = createDescriptorSetLayout(reflection.shaderLayout);
    pass.pipelineLayout = createPipelineLayout(pass.descriptorSetLayout, reflection.pushConstantByteSize, pipelineLayoutStageFlags);
    pass.pushConstantSize = reflection.pushConstantByteSize;

    std::vector<VkVertexInputAttributeDescription> attributes;
    uint32_t currentOffset = 0;

    for (uint32_t location = 0; location < VERTEX_INPUT_ATTRIBUTE_COUNT; location++) {
        if (bool(vertexInputFlagPerLocation[location] & reflection.vertexInputFlags)) {
            VkVertexInputAttributeDescription attribute;
            attribute.location = location;
            attribute.binding = 0;
            attribute.format = vertexInputFormatsPerLocation[(size_t)location];
            attribute.offset = currentOffset;
            attributes.push_back(attribute);
        }
        //vertex buffer has attributes even if not used
        currentOffset += vertexInputBytePerLocation[(size_t)location];
    }

    VkVertexInputBindingDescription vertexBinding = {};
    vertexBinding.binding = 0;
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    switch (desc.vertexFormat) {
        case VertexFormat::Full: vertexBinding.stride = currentOffset; break;
        case VertexFormat::PositionOnly: vertexBinding.stride = vertexInputBytePerLocation[0]; break; //location 0 is position
        default: vertexBinding.stride = currentOffset; std::cout << "Warning: unknown vertex format\n"; break;
    }
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.pNext = nullptr;
    vertexInputInfo.flags = 0;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexBinding;
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attributes.size();
    vertexInputInfo.pVertexAttributeDescriptions = attributes.data();

    pass.vulkanRenderPass = createVulkanRenderPass(desc.attachments);

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;
    viewportState.flags = 0;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr; //ignored as viewport is dynamic
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;  //ignored as viewport is dynamic

    //only global blending state for all attachments
    //currently only no blending and additive supported    
    VkPipelineColorBlendAttachmentState blendingAttachment = {};
    blendingAttachment.blendEnable = desc.blending != BlendState::None ? VK_TRUE : VK_FALSE;
    blendingAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendingAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    blendingAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendingAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendingAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendingAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendingAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    std::vector<VkPipelineColorBlendAttachmentState> blendingAttachments;
    for (const auto& attachment : desc.attachments) {
        if (!isDepthFormat(attachment.format)) {
            blendingAttachments.push_back(blendingAttachment);
        }
    }

    VkPipelineColorBlendStateCreateInfo blending = {};
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.pNext = nullptr;
    blending.flags = 0;
    blending.logicOpEnable = VK_FALSE;
    blending.logicOp = VK_LOGIC_OP_NO_OP;
    blending.attachmentCount = (uint32_t)blendingAttachments.size();
    blending.pAttachments = blendingAttachments.data();
    blending.blendConstants[0] = 0.f;
    blending.blendConstants[1] = 0.f;
    blending.blendConstants[2] = 0.f;
    blending.blendConstants[3] = 0.f;

    const auto rasterizationState = createRasterizationState(desc.rasterization);
    auto multisamplingState = createDefaultMultisamplingInfo();
    const auto depthStencilState = createDepthStencilState(desc.depthTest);

    VkPipelineTessellationStateCreateInfo tesselationState = {};
    VkPipelineTessellationStateCreateInfo* pTesselationState;
    if (desc.shaderDescriptions.tesselationControl.has_value()) {
        tesselationState = createTesselationState(desc.patchControlPoints);
        pTesselationState = &tesselationState;
    }
    else {
        pTesselationState = nullptr;
    }

    auto inputAssemblyState = createDefaultInputAssemblyInfo();

    if (desc.rasterization.mode == RasterizationeMode::Line) {
        inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    }
    else if (desc.rasterization.mode == RasterizationeMode::Point) {
        inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }

    std::vector<VkDynamicState> dynamicStates;
    dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

    VkPipelineDynamicStateCreateInfo dynamicStateInfo;
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.pNext = nullptr;
    dynamicStateInfo.flags = 0;
    dynamicStateInfo.dynamicStateCount = (uint32_t)dynamicStates.size();
    dynamicStateInfo.pDynamicStates = dynamicStates.data();


    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.flags = 0;
    pipelineInfo.stageCount = (uint32_t)stages.size();
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineInfo.pTessellationState = pTesselationState;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizationState.baseInfo;
    pipelineInfo.pMultisampleState = &multisamplingState;
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.pColorBlendState = &blending;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.layout = pass.pipelineLayout;
    pipelineInfo.renderPass = pass.vulkanRenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = 0;

    auto result = vkCreateGraphicsPipelines(vkContext.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pass.pipeline);
    checkVulkanResult(result);

    //shader modules aren't needed anymore
    vkDestroyShaderModule(vkContext.device, vertexModule, nullptr);
    vkDestroyShaderModule(vkContext.device, fragmentModule, nullptr);
    if (geometryModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(vkContext.device, geometryModule, nullptr);
    }
    if (tesselationControlModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(vkContext.device, tesselationControlModule, nullptr);
        vkDestroyShaderModule(vkContext.device, tesselationEvaluationModule, nullptr);
    }

    //clear values    
    for (const auto& attachment : desc.attachments) {
        if (!isDepthFormat(attachment.format)) {
            VkClearValue colorClear = {};
            colorClear.color = { 0, 0, 0, 0 };
            pass.clearValues.push_back(colorClear);
        }
        else {
            VkClearValue depthClear = {};
            depthClear.depthStencil.depth = 0.f;
            pass.clearValues.push_back(depthClear);
        }
    }

    const auto setSizes = descriptorSetAllocationSizeFromShaderLayout(reflection.shaderLayout);
    pass.descriptorSets[0] = allocateDescriptorSet(pass.descriptorSetLayout, setSizes);
    pass.descriptorSets[1] = allocateDescriptorSet(pass.descriptorSetLayout, setSizes);

    return pass;
}

bool validateAttachmentFormatsAreCompatible(const ImageFormat a, const ImageFormat b) {
    return imageFormatToVkAspectFlagBits(a) == imageFormatToVkAspectFlagBits(b);
}

VkShaderModule RenderBackend::createShaderModule(const std::vector<uint32_t>& code) {

    VkShaderModuleCreateInfo moduleInfo;
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.pNext = nullptr;
    moduleInfo.flags = 0;
    moduleInfo.codeSize = code.size() * sizeof(uint32_t);
    moduleInfo.pCode = code.data();

    VkShaderModule shaderModule;
    auto res = vkCreateShaderModule(vkContext.device, &moduleInfo, nullptr, &shaderModule);
    checkVulkanResult(res);

    return shaderModule;
}

VkPipelineShaderStageCreateInfo RenderBackend::createPipelineShaderStageInfos(
    const VkShaderModule module, 
    const VkShaderStageFlagBits stage,
    const std::vector<SpecialisationConstant>& specialisationInfo, 
    VulkanShaderCreateAdditionalStructs* outAdditionalInfo) {

    assert(outAdditionalInfo != nullptr);
    size_t specialisationCount = specialisationInfo.size();

    VkPipelineShaderStageCreateInfo createInfos;
    createInfos.sType                  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfos.pNext                  = nullptr;
    createInfos.flags                  = 0;
    createInfos.stage                  = stage;
    createInfos.module                 = module;
    createInfos.pName                  = "main";
    createInfos.pSpecializationInfo    = nullptr;

    if (specialisationInfo.size() > 0) {

        outAdditionalInfo->specilisationMap.resize(specialisationCount);

        size_t currentOffset = 0;
        for (uint32_t i = 0; i < outAdditionalInfo->specilisationMap.size(); i++) {
            const auto constant = specialisationInfo[i];

            outAdditionalInfo->specilisationMap[i].constantID = constant.location;
            outAdditionalInfo->specilisationMap[i].offset = (uint32_t)currentOffset;
            outAdditionalInfo->specilisationMap[i].size = constant.data.size();

            currentOffset += constant.data.size();
        }

        {
            //make space
            outAdditionalInfo->specialisationData.resize(currentOffset);

            //reset offset
            currentOffset = 0;

            //copy data into place
            for (const auto& constant : specialisationInfo) {
                memcpy(outAdditionalInfo->specialisationData.data() + currentOffset, constant.data.data(), constant.data.size());
                currentOffset += constant.data.size();
            }
        }
        

        outAdditionalInfo->specialisationInfo.dataSize      = specialisationCount * sizeof(int);
        outAdditionalInfo->specialisationInfo.mapEntryCount = (uint32_t)specialisationCount;
        outAdditionalInfo->specialisationInfo.pData         = outAdditionalInfo->specialisationData.data();
        outAdditionalInfo->specialisationInfo.pMapEntries   = outAdditionalInfo->specilisationMap.data();

        createInfos.pSpecializationInfo = &outAdditionalInfo->specialisationInfo;
    }

    return createInfos;
}

VkPipelineInputAssemblyStateCreateInfo RenderBackend::createDefaultInputAssemblyInfo() {
    VkPipelineInputAssemblyStateCreateInfo info;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    info.primitiveRestartEnable = VK_FALSE;
    return info;
}

VkPipelineTessellationStateCreateInfo RenderBackend::createTesselationState(const uint32_t patchControlPoints) {
    VkPipelineTessellationStateCreateInfo info;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.patchControlPoints = patchControlPoints;
    return info;
}

VulkanRasterizationStateCreateInfo RenderBackend::createRasterizationState(const RasterizationConfig& raster) {

    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    switch (raster.mode) {
    case RasterizationeMode::Fill:  polygonMode = VK_POLYGON_MODE_FILL; break;
    case RasterizationeMode::Line:  polygonMode = VK_POLYGON_MODE_LINE; break;
    case RasterizationeMode::Point:  polygonMode = VK_POLYGON_MODE_POINT; break;
    default: std::cout << "RenderBackend::createRasterizationState: Unknown RasterizationeMode\n"; break;
    };

    VkCullModeFlags cullFlags = VK_CULL_MODE_NONE;
    switch (raster.cullMode) {
    case CullMode::None:  cullFlags = VK_CULL_MODE_NONE; break;
    case CullMode::Front:  cullFlags = VK_CULL_MODE_FRONT_BIT; break;
    case CullMode::Back:  cullFlags = VK_CULL_MODE_BACK_BIT; break;
    default: std::cout << "RenderBackend::createRasterizationState unknown CullMode\n"; break;
    };

	VulkanRasterizationStateCreateInfo vkRaster = {};

	vkRaster.baseInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    vkRaster.baseInfo.pNext = nullptr;
    vkRaster.baseInfo.flags = 0;
    vkRaster.baseInfo.depthClampEnable = raster.clampDepth;
    vkRaster.baseInfo.rasterizerDiscardEnable = VK_FALSE;
    vkRaster.baseInfo.polygonMode = polygonMode;
    vkRaster.baseInfo.cullMode = cullFlags;
    vkRaster.baseInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    vkRaster.baseInfo.depthBiasEnable = VK_FALSE;
    vkRaster.baseInfo.depthBiasConstantFactor = 0.f;
    vkRaster.baseInfo.depthBiasClamp = 0.f;
    vkRaster.baseInfo.depthBiasSlopeFactor = 0.f;
    vkRaster.baseInfo.lineWidth = 1.f;

    if (raster.conservative) {
        vkRaster.conservativeInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_CONSERVATIVE_STATE_CREATE_INFO_EXT;
        vkRaster.conservativeInfo.pNext = nullptr;
        vkRaster.conservativeInfo.flags = 0;
        vkRaster.conservativeInfo.conservativeRasterizationMode = VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT;

        vkRaster.baseInfo.pNext = &vkRaster.conservativeInfo;
    }

    return vkRaster;
}

VkPipelineMultisampleStateCreateInfo RenderBackend::createDefaultMultisamplingInfo() {
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.pNext = nullptr;
    multisampling.flags = 0;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.minSampleShading = 0.f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;
    return multisampling;
}

VkPipelineDepthStencilStateCreateInfo RenderBackend::createDepthStencilState(const DepthTest& depthTest) {

    // no stencil used, infos don't matter
    VkStencilOpState stencilInfoDummy = {};
    stencilInfoDummy.failOp = VK_STENCIL_OP_KEEP;
    stencilInfoDummy.passOp = VK_STENCIL_OP_KEEP;
    stencilInfoDummy.depthFailOp = VK_STENCIL_OP_KEEP;
    stencilInfoDummy.compareOp = VK_COMPARE_OP_NEVER;
    stencilInfoDummy.compareMask = 0;
    stencilInfoDummy.writeMask = 0;
    stencilInfoDummy.reference = 0;

    VkPipelineDepthStencilStateCreateInfo depthInfo = {};
    VkCompareOp compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    switch (depthTest.function) {
    case DepthFunction::Always: compareOp = VK_COMPARE_OP_ALWAYS; break;
    case DepthFunction::Equal: compareOp = VK_COMPARE_OP_EQUAL; break;
    case DepthFunction::Greater: compareOp = VK_COMPARE_OP_GREATER; break;
    case DepthFunction::GreaterEqual : compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; break;
    case DepthFunction::Less: compareOp = VK_COMPARE_OP_LESS; break;
    case DepthFunction::LessEqual: compareOp = VK_COMPARE_OP_LESS_OR_EQUAL; break;
    case DepthFunction::Never : compareOp = VK_COMPARE_OP_NEVER; break;
    default: std::cout << "RenderBackend::createDepthStencilState unknown DepthFunction\n"; break;
    }

    depthInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthInfo.pNext = nullptr;
    depthInfo.flags = 0;
    depthInfo.depthTestEnable = depthTest.function != DepthFunction::Always;
    depthInfo.depthWriteEnable = depthTest.write;
    depthInfo.depthCompareOp = compareOp;
    depthInfo.depthBoundsTestEnable = VK_FALSE;
    depthInfo.stencilTestEnable = VK_FALSE;
    depthInfo.front = stencilInfoDummy;
    depthInfo.back = stencilInfoDummy;
    depthInfo.minDepthBounds = 0.f;
    depthInfo.maxDepthBounds = 1.f;
    return depthInfo;
}

void RenderBackend::barriersCommand(const VkCommandBuffer commandBuffer, 
    const std::vector<VkImageMemoryBarrier>& imageBarriers, const std::vector<VkBufferMemoryBarrier>& memoryBarriers) {

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 
        (uint32_t)memoryBarriers.size(), memoryBarriers.data(), (uint32_t)imageBarriers.size(), imageBarriers.data());
}

std::vector<VkImageMemoryBarrier> RenderBackend::createImageBarriers(Image& image, const VkImageLayout newLayout,
    const VkAccessFlags dstAccess, const uint32_t baseMip, const uint32_t mipLevels) {

    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    const bool isDepthImage = isVulkanDepthFormat(image.format);
    if (isDepthImage) {
        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    std::vector<VkImageMemoryBarrier> barriers;

    const uint32_t layerCount = computeImageLayerCount(image.desc.type);

    // first barrier 
    VkImageMemoryBarrier firstBarrier;
    firstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    firstBarrier.pNext = nullptr;
    firstBarrier.srcAccessMask = image.currentAccess;
    firstBarrier.dstAccessMask = dstAccess;
    firstBarrier.oldLayout = image.layoutPerMip[baseMip];
    firstBarrier.newLayout = newLayout;
    firstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    firstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    firstBarrier.image = image.vulkanHandle;
    firstBarrier.subresourceRange.aspectMask = aspectFlags;
    firstBarrier.subresourceRange.baseMipLevel = baseMip;
    firstBarrier.subresourceRange.levelCount = 1;
    firstBarrier.subresourceRange.baseArrayLayer = 0;
    firstBarrier.subresourceRange.layerCount = layerCount;
    barriers.push_back(firstBarrier);

    // add subsequent mip level barriers
    for (uint32_t i = 1; i < mipLevels; i++) {

        // same mip layout: extens subresource range
        uint32_t mipLevel = baseMip + i;
        if (image.layoutPerMip[mipLevel] == barriers.back().oldLayout) {
            barriers.back().subresourceRange.levelCount++;
        }

        // different mip layout: new barrier
        else {
            VkImageMemoryBarrier barrier;
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = nullptr;
            barrier.srcAccessMask = image.currentAccess;
            barrier.dstAccessMask = dstAccess;
            barrier.oldLayout = image.layoutPerMip[mipLevel];
            barrier.newLayout = newLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image.vulkanHandle;
            barrier.subresourceRange.aspectMask = aspectFlags;
            barrier.subresourceRange.baseMipLevel = mipLevel;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = layerCount;

            barriers.push_back(barrier);
        }
    }

    // update image properties
    for (uint32_t i = baseMip; i < baseMip + mipLevels; i++) {
        image.layoutPerMip[i] = newLayout;
    }
    image.currentAccess = dstAccess;
    image.currentlyWriting = false;

    return barriers;
}

VkBufferMemoryBarrier RenderBackend::createBufferBarrier(const Buffer& buffer, const VkAccessFlagBits srcAccess, const VkAccessFlagBits dstAccess) {
    VkBufferMemoryBarrier barrier;
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.pNext = nullptr;
    barrier.srcAccessMask = srcAccess;
    barrier.srcAccessMask = dstAccess;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer.vulkanHandle;
    barrier.offset = 0;
    barrier.size = buffer.size;
    return barrier;
}

VkSemaphore RenderBackend::createSemaphore() {

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.flags = 0;

    VkSemaphore semaphore;
    auto res = vkCreateSemaphore(vkContext.device, &semaphoreInfo, nullptr, &semaphore);
    checkVulkanResult(res);

    return semaphore;
}

VkFence RenderBackend::createFence() {

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkFence fence;
    auto res = vkCreateFence(vkContext.device, &fenceInfo, nullptr, &fence);
    checkVulkanResult(res);

    return fence;
}

VkQueryPool RenderBackend::createQueryPool(const VkQueryType queryType, const uint32_t queryCount) {

    VkQueryPoolCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.queryType = queryType;
    createInfo.queryCount = queryCount;
    createInfo.pipelineStatistics = 0; // pipeline queries not handled for now

    VkQueryPool pool;
    const auto res = vkCreateQueryPool(vkContext.device, &createInfo, nullptr, &pool);
    assert(res == VK_SUCCESS);

    vkResetQueryPool(vkContext.device, pool, 0, queryCount);

    return pool;
}

void RenderBackend::resetTimestampQueryPool(const int poolIndex) {
    m_timestampQueriesPerFrame[poolIndex].resize(0);
    vkResetQueryPool(vkContext.device, m_timestampQueryPools[poolIndex], 0, m_timestampQueryCounts[m_frameIndexMod2]);
    m_timestampQueryCounts[m_frameIndexMod2] = 0;
}

uint32_t RenderBackend::issueTimestampQuery(const VkCommandBuffer cmdBuffer, const int poolIndex) {
    const uint32_t query = m_timestampQueryCounts[m_frameIndexMod2];
    vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_timestampQueryPools[poolIndex], query);
    m_timestampQueryCounts[m_frameIndexMod2]++;
    return query;
}

void RenderBackend::destroyImage(const ImageHandle handle) {
    m_freeImageHandles.push_back(handle);
    const Image& image = getImageRef(handle);
    destroyImageInternal(image);
}

void RenderBackend::destroyImageInternal(const Image& image) {

    if (bool(image.desc.usageFlags & ImageUsageFlags::Sampled)) {
        m_globalTextureArrayDescriptorSetFreeTextureIndices.push_back(image.globalDescriptorSetIndex);
    }

    for (const auto& view : image.viewPerMip) {
        vkDestroyImageView(vkContext.device, view, nullptr);
    }

    // swapchain images have no manualy allocated memory
    // they are deleted by the swapchain
    // view has to be destroyed manually though
    if (!image.isSwapchainImage) {
        m_vkAllocator.free(image.memory);
        vkDestroyImage(vkContext.device, image.vulkanHandle, nullptr);
    }
}

void RenderBackend::destroyBuffer(const Buffer& buffer) {
    vkDestroyBuffer(vkContext.device, buffer.vulkanHandle, nullptr);
    m_vkAllocator.free(buffer.memory);
}

void RenderBackend::destroyMesh(const Mesh& mesh) {
    destroyBuffer(mesh.vertexBuffer);
    destroyBuffer(mesh.indexBuffer);
}

void RenderBackend::destroyGraphicPass(const GraphicPass& pass) {
    vkDestroyRenderPass(vkContext.device, pass.vulkanRenderPass, nullptr);
    vkDestroyPipelineLayout(vkContext.device, pass.pipelineLayout, nullptr);
    vkDestroyPipeline(vkContext.device, pass.pipeline, nullptr);
    vkDestroyDescriptorSetLayout(vkContext.device, pass.descriptorSetLayout, nullptr);
}

void RenderBackend::destroyComputePass(const ComputePass& pass) {
    vkDestroyPipelineLayout(vkContext.device, pass.pipelineLayout, nullptr);
    vkDestroyPipeline(vkContext.device, pass.pipeline, nullptr);
    vkDestroyDescriptorSetLayout(vkContext.device, pass.descriptorSetLayout, nullptr);
}