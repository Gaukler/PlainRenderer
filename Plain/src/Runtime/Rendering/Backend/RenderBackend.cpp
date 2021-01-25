#include "pch.h"
#include "RenderBackend.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <vulkan/vulkan.h>

#include "SpirvReflection.h"
#include "VertexInput.h"
#include "ShaderIO.h"
#include "Utilities/GeneralUtils.h"
#include "Utilities/MathUtils.h"
#include "VertexInputVulkan.h"
#include "VulkanImageFormats.h"

//disable ImGui warnings
#pragma warning( push )
#pragma warning( disable : 26495 26812)

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>

//definition of extern variable from header
RenderBackend gRenderBackend;

//reenable warnings
#pragma warning( pop )

//vulkan uses enums, which result in a warning every time they are used
//this warning is disabled for this entire file
#pragma warning( disable : 26812) //C26812: Prefer 'enum class' over 'enum' 

const uint32_t maxTextureCount = 1000;

/*
==================

RenderPasses

==================
*/

bool RenderPasses::isGraphicPassHandle(const RenderPassHandle handle) {
    //checks first bit
    const uint32_t upperBit = (uint32_t)1 << 31;
    return handle.index & upperBit;
}

RenderPassHandle RenderPasses::addGraphicPass(const GraphicPass pass) {
    uint32_t index = (uint32_t)m_graphicPasses.size();
    m_graphicPasses.push_back(pass);
    return indexToGraphicPassHandle(index);
}

RenderPassHandle RenderPasses::addComputePass(const ComputePass pass) {
    uint32_t index = (uint32_t)m_computePasses.size();
    m_computePasses.push_back(pass);
    return indexToComputePassHandle(index);
}

uint32_t RenderPasses::getNGraphicPasses() {
    return (uint32_t)m_graphicPasses.size();
}

uint32_t RenderPasses::getNComputePasses(){
    return (uint32_t)m_computePasses.size();
}

GraphicPass& RenderPasses::getGraphicPassRefByHandle(const RenderPassHandle handle) {
    assert(isGraphicPassHandle(handle));
    return m_graphicPasses[graphicPassHandleToIndex(handle)];
}

ComputePass& RenderPasses::getComputePassRefByHandle(const RenderPassHandle handle) {
    assert(!isGraphicPassHandle(handle));
    return m_computePasses[computePassHandleToIndex(handle)];
}

GraphicPass& RenderPasses::getGraphicPassRefByIndex(const uint32_t index) {
    return m_graphicPasses[index];
}

ComputePass& RenderPasses::getComputePassRefByIndex(const uint32_t index) {
    return m_computePasses[index];
}

uint32_t RenderPasses::graphicPassHandleToIndex(const RenderPassHandle handle) {
    //set first bit to 0
    const uint32_t noUpperBit = ~(1 << 31);
    return handle.index & noUpperBit;
}

uint32_t RenderPasses::computePassHandleToIndex(const RenderPassHandle handle) {
    //first bit already 0, just return
    return handle.index;
}

RenderPassHandle RenderPasses::indexToGraphicPassHandle(const uint32_t index) {
    //set first bit to 1 and cast
    const uint32_t upperBit = (uint32_t)1 << 31;
    return { index | upperBit };
}

RenderPassHandle RenderPasses::indexToComputePassHandle(const uint32_t index) {
    //first bit should already be 0, just cast
    return { index };
}

/*
==================

RenderBackend

==================
*/

//callback needs a lot of parameters which are not used
//disable warning for this function
#pragma warning( push )
#pragma warning( disable : 4100 ) //4100: unreference formal parameter

VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(
    VkDebugReportFlagsEXT       flags,
    VkDebugReportObjectTypeEXT  objectType,
    uint64_t                    object,
    size_t                      location,
    int32_t                     messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData)
{
    std::cerr << pMessage << std::endl;
    return VK_FALSE;
}

//parameter result not used in release mode, keep warning disabled
void checkVulkanResult(const VkResult result) {
    assert(result == VK_SUCCESS);
}

//reenable warnings
#pragma warning( pop )

void RenderBackend::setup(GLFWwindow* window) {

    createVulkanInstance();
    createSurface(window);
    if (m_useValidationLayers) {
        m_debugCallback = setupDebugCallbacks();
    }
    pickPhysicalDevice();
    getQueueFamilies(vkContext.physicalDevice, &vkContext.queueFamilies);
    createLogicalDevice();

    vkGetDeviceQueue(vkContext.device, vkContext.queueFamilies.graphicsQueueIndex, 0,       &vkContext.graphicQueue);
    vkGetDeviceQueue(vkContext.device, vkContext.queueFamilies.presentationQueueIndex, 0,   &vkContext.presentQueue);
    vkGetDeviceQueue(vkContext.device, vkContext.queueFamilies.transferQueueFamilyIndex, 0, &vkContext.transferQueue);
    vkGetDeviceQueue(vkContext.device, vkContext.queueFamilies.computeQueueIndex, 0,        &vkContext.computeQueue);

    chooseSurfaceFormat();
    createSwapChain();

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    getSwapchainImages((uint32_t)width, (uint32_t)height);

    acquireDebugUtilsExtFunctionsPointers();

    m_commandPool = createCommandPool(vkContext.queueFamilies.graphicsQueueIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    m_transientCommandPool = createCommandPool(vkContext.queueFamilies.transferQueueFamilyIndex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    m_swapchain.imageAvaible = createSemaphore();
    m_renderFinishedSemaphore = createSemaphore();
    m_renderFinishedFence = createFence();

    m_vkAllocator.create();

    //staging buffer
    {
        const auto stagingBufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        const auto stagingBufferMemoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        const std::vector<uint32_t> stagingBufferQueueFamilies = { vkContext.queueFamilies.transferQueueFamilyIndex };
        m_stagingBuffer = createBufferInternal(
            m_stagingBufferSize, 
            stagingBufferQueueFamilies, 
            stagingBufferUsageFlags, 
            stagingBufferMemoryFlags);
    }

    m_commandBuffers[0] = allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    m_commandBuffers[1] = allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    initGlobalTextureArrayDescriptorSetLayout();
	initGlobalTextureArrayDescriptorSet();
    setupImgui(window);

    //query pools
    m_timestampQueryPool = createQueryPool(VK_QUERY_TYPE_TIMESTAMP, m_timestampQueryPoolQueryCount);
}

void RenderBackend::shutdown() {

    waitForRenderFinished();

    if (m_useValidationLayers) {
        PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
            reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>
            (vkGetInstanceProcAddr(vkContext.vulkanInstance, "vkDestroyDebugReportCallbackEXT"));
        vkDestroyDebugReportCallbackEXT(vkContext.vulkanInstance, m_debugCallback, nullptr);
    }

    //destroy resources
    for (uint32_t i = 0; i < m_images.size(); i++) {
        destroyImage({ i });
    }

    for (uint32_t i = 0; i < m_renderPasses.getNGraphicPasses(); i++) {
        destroyGraphicPass(m_renderPasses.getGraphicPassRefByIndex(i));
    }
    for (uint32_t i = 0; i < m_renderPasses.getNComputePasses(); i++) {
        destroyComputePass(m_renderPasses.getComputePassRefByIndex(i));
    }
    for (const auto& mesh : m_meshes) {
        destroyMesh(mesh);
    }
    for (const auto& mesh : m_dynamicMeshes) {
        destroyDynamicMesh(mesh);
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
    for (const auto& framebuffer : m_framebuffers) {
        destroyFramebuffer(framebuffer);
    }
    vkDestroyDescriptorSetLayout(vkContext.device, m_globalTextureArrayDescriporSetLayout, nullptr);

    //destroy swapchain
    vkDestroySwapchainKHR(vkContext.device, m_swapchain.vulkanHandle, nullptr);
    vkDestroySurfaceKHR(vkContext.vulkanInstance, m_swapchain.surface, nullptr);

    m_vkAllocator.destroy();

    /*
    destroy ui
    */
    for (const auto& framebuffer : m_ui.framebuffers) {
        vkDestroyFramebuffer(vkContext.device, framebuffer, nullptr);
    }
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

    vkDestroySemaphore(vkContext.device, m_renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(vkContext.device, m_swapchain.imageAvaible, nullptr);

    vkDestroyQueryPool(vkContext.device, m_timestampQueryPool, nullptr);

    vkDestroyFence(vkContext.device, m_renderFinishedFence, nullptr);
    vkDestroyDevice(vkContext.device, nullptr);
    vkDestroyInstance(vkContext.vulkanInstance, nullptr);
}

void RenderBackend::recreateSwapchain(const uint32_t width, const uint32_t height, GLFWwindow* window) {

    auto result = vkDeviceWaitIdle(vkContext.device);
    checkVulkanResult(result);

    /*
    destroy swapchain and views
    */
    for (const auto& imageHandle : m_swapchain.imageHandles) {
        destroyImage(imageHandle);
    }
    vkDestroySwapchainKHR(vkContext.device, m_swapchain.vulkanHandle, nullptr);
    vkDestroySurfaceKHR(vkContext.vulkanInstance, m_swapchain.surface, nullptr);
    
    /*
    recreate
    */
    createSurface(window);
    /*
    queue families must revalidate present support for new surface
    */
    getQueueFamilies(vkContext.physicalDevice, &vkContext.queueFamilies);
    createSwapChain();
    getSwapchainImages(width, height);

    /*
    recreate imgui pass framebuffer
    */
    
    VkExtent2D extent;
    extent.width = width;
    extent.height = height;

    assert(m_ui.framebuffers.size() == m_ui.passBeginInfos.size());
    assert(m_ui.framebuffers.size() == m_swapchain.imageHandles.size());
    for (uint32_t i = 0; i < m_ui.framebuffers.size(); i++) {

        VkFramebuffer oldBuffer = m_ui.framebuffers[i];
        vkDestroyFramebuffer(vkContext.device, oldBuffer, nullptr);

        FramebufferTarget uiTarget;
        uiTarget.image = m_swapchain.imageHandles[i];
        uiTarget.mipLevel = 0;
        VkFramebuffer newBuffer = createVulkanFramebuffer({ uiTarget }, m_ui.renderPass);
        m_ui.framebuffers[i] = newBuffer;
        m_ui.passBeginInfos[i].framebuffer = newBuffer;
        m_ui.passBeginInfos[i].renderArea.extent = extent;
    }
}

void RenderBackend::updateShaderCode() {

    m_shaderFileManager.updateFileLastChangeTimes();

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

    /*
    recreate image
    */
    for (const auto image : images) {
        m_images[image.index].desc.width = width;
        m_images[image.index].desc.height = height;
        const auto imageDesc = m_images[image.index].desc;
        destroyImage(image);
        ImageHandle newHandle = createImage(imageDesc);
        assert(newHandle.index == image.index);
    }

    //recreate framebuffer that use image    
    VkExtent2D extent;
    extent.width = width;
    extent.height = height;

    VkRect2D rect = {};
    rect.extent = extent;
    rect.offset = { 0, 0 };

    for (Framebuffer& framebuffer : m_framebuffers) {
        bool mustBeResized = false;
        
        //check if one of the images from framebuffer uses resized image
        for (const FramebufferTarget target : framebuffer.desc.targets) {
            const ImageHandle targetImage = target.image;
            for (const ImageHandle resizedImage : images) {
                if (targetImage.index == resizedImage.index) {
                    mustBeResized = true;
                    break;
                }
            }
            if (mustBeResized) {
                break;
            }
        }

        if (mustBeResized) {
            destroyFramebuffer(framebuffer);
            const GraphicPass graphicPass = m_renderPasses.getGraphicPassRefByHandle(framebuffer.desc.compatibleRenderpass);
            framebuffer.vkHandle = createVulkanFramebuffer(framebuffer.desc.targets, graphicPass.vulkanRenderPass);
        }
    }
}

void RenderBackend::newFrame() {

    //wait for previous frame to render so resources are avaible
    auto res = vkWaitForFences(vkContext.device, 1, &m_renderFinishedFence, VK_TRUE, UINT64_MAX);
    assert(res == VK_SUCCESS);
    res = vkResetFences(vkContext.device, 1, &m_renderFinishedFence);
    assert(res == VK_SUCCESS);

    m_renderPassExecutions.clear();
    m_swapchainInputImageHandle.index = VK_NULL_HANDLE;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void RenderBackend::startDrawcallRecording() {
    //iterate over graphic passes that will be executed
    for (const RenderPassExecution execution : m_renderPassExecutions) {
        const RenderPassHandle passHandle = execution.handle;

        //only need graphic passes
        if (!m_renderPasses.isGraphicPassHandle(passHandle)) {
            continue;
        }

        const GraphicPass pass = m_renderPasses.getGraphicPassRefByHandle(passHandle);

        const auto res = vkResetCommandBuffer(pass.meshCommandBuffer, 0);
        assert(res == VK_SUCCESS);

        const Framebuffer framebuffer = m_framebuffers[execution.framebuffer.index];

        VkCommandBufferInheritanceInfo inheritanceInfo;
        inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        inheritanceInfo.pNext = nullptr;
        inheritanceInfo.renderPass = pass.vulkanRenderPass;
        inheritanceInfo.subpass = 0;
        inheritanceInfo.framebuffer = framebuffer.vkHandle;
        inheritanceInfo.occlusionQueryEnable = false;
        inheritanceInfo.queryFlags = 0;
        inheritanceInfo.pipelineStatistics = 0;

        VkCommandBufferBeginInfo cmdBeginInfo;
        cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBeginInfo.pNext = nullptr;
        cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        cmdBeginInfo.pInheritanceInfo = &inheritanceInfo;

        vkBeginCommandBuffer(pass.meshCommandBuffer, &cmdBeginInfo);
        vkCmdBindPipeline(pass.meshCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipeline);

        const glm::ivec2 resolution = resolutionFromFramebufferTargets(framebuffer.desc.targets);

        //set viewport
        {
            VkViewport viewport;
            viewport.x = 0;
            viewport.y = 0;
            viewport.width = (float)resolution.x;
            viewport.height = (float)resolution.y;
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;

            vkCmdSetViewport(pass.meshCommandBuffer, 0, 1, &viewport);
        }
        //set scissor
        {
            VkRect2D scissor;
            scissor.offset = { 0, 0 };
            scissor.extent.width = resolution.x;
            scissor.extent.height = resolution.y;

            vkCmdSetScissor(pass.meshCommandBuffer, 0, 1, &scissor);
        }
    }
}

void RenderBackend::setRenderPassExecution(const RenderPassExecution& execution) {

    if (m_renderPasses.isGraphicPassHandle(execution.handle)) {
        if (execution.framebuffer.index == invalidIndex) {
            std::cout << "Renderpass execution is missing framebuffer, skipping execution\n";
            return;
        }
        const Framebuffer framebuffer = m_framebuffers[execution.framebuffer.index];
        if (!validateFramebufferTargetGraphicPassCombination(framebuffer.desc.targets, execution.handle)) {
            std::cout << "Framebuffer and renderpass are incompatible, skipping execution\n";
            return;
        }
        updateDescriptorSet(m_renderPasses.getGraphicPassRefByHandle(execution.handle).descriptorSet, execution.resources);
    }
    else {
        updateDescriptorSet(m_renderPasses.getComputePassRefByHandle(execution.handle).descriptorSet, execution.resources);
    }
    
    m_renderPassExecutions.push_back(execution);
}

void RenderBackend::drawMeshes(
    const std::vector<MeshHandle> meshHandles, 
    const std::vector<std::array<glm::mat4, 2>>& primarySecondaryMatrices, 
    const RenderPassHandle passHandle) {
    auto& pass = m_renderPasses.getGraphicPassRefByHandle(passHandle);

    if (meshHandles.size() != primarySecondaryMatrices.size()) {
        std::cout << "Error: drawMeshes handle and matrix count does not match\n";
    }

	VkShaderStageFlags pushConstantStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	if (pass.graphicPassDesc.shaderDescriptions.geometry.has_value()) {
		pushConstantStageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
	}

    for (uint32_t i = 0; i < std::min(meshHandles.size(), primarySecondaryMatrices.size()); i++) {

        const auto mesh = m_meshes[meshHandles[i].index];

        //vertex/index buffers            
        VkDeviceSize offset[] = { 0 };
        vkCmdBindVertexBuffers(pass.meshCommandBuffer, 0, 1, &mesh.vertexBuffer.vulkanHandle, offset);
        vkCmdBindIndexBuffer(pass.meshCommandBuffer, mesh.indexBuffer.vulkanHandle, offset[0], mesh.indexPrecision);

		struct PushConstantBlock {
			glm::mat4 matrices[2];
			int32_t indices[3];
		};
		PushConstantBlock block;
		block.matrices[0] = primarySecondaryMatrices[i][0];
		block.matrices[1] = primarySecondaryMatrices[i][1];
		block.indices[0] = mesh.materialTextureIndices.albedo;
		block.indices[1] = mesh.materialTextureIndices.normal;
		block.indices[2] = mesh.materialTextureIndices.specular;

        //update push constants
        vkCmdPushConstants(
            pass.meshCommandBuffer, 
            pass.pipelineLayout, 
            pushConstantStageFlags, 
            0, 
            sizeof(block),
            &block);

        //materials            
        VkDescriptorSet sets[3] = { m_globalDescriptorSet, pass.descriptorSet, m_globalTextureArrayDescriptorSet };
        vkCmdBindDescriptorSets(pass.meshCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipelineLayout, 0, 3, sets, 0, nullptr);

        vkCmdDrawIndexed(pass.meshCommandBuffer, mesh.indexCount, 1, 0, 0, 0);
    }
}

void RenderBackend::drawDynamicMeshes(
    const std::vector<DynamicMeshHandle> meshHandles,
    const std::vector<std::array<glm::mat4, 2>>& primarySecondaryMatrices, 
    const RenderPassHandle passHandle) {

    if (meshHandles.size() != primarySecondaryMatrices.size()) {
        std::cout << "Error: drawMeshes handle and modelMatrix count does not match\n";
    }

    auto& pass = m_renderPasses.getGraphicPassRefByHandle(passHandle);

	VkShaderStageFlags pipelineLayoutStageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	if (pass.graphicPassDesc.shaderDescriptions.geometry.has_value()) {
		pipelineLayoutStageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
	}

    for (uint32_t i = 0; i < std::min(meshHandles.size(), primarySecondaryMatrices.size()); i++) {

        const auto meshHandle = meshHandles[i];
        const auto mesh = m_dynamicMeshes[meshHandle.index];

        //vertex/index buffers
        VkDeviceSize offset[] = { 0 };
        vkCmdBindVertexBuffers(pass.meshCommandBuffer, 0, 1, &mesh.vertexBuffer.vulkanHandle, offset);
        vkCmdBindIndexBuffer(pass.meshCommandBuffer, mesh.indexBuffer.vulkanHandle, offset[0], VK_INDEX_TYPE_UINT32);

        //update push constants
        const auto& matrices = primarySecondaryMatrices[i];
        vkCmdPushConstants(pass.meshCommandBuffer, pass.pipelineLayout, pipelineLayoutStageFlags, 0, sizeof(matrices), &matrices);

        vkCmdDrawIndexed(pass.meshCommandBuffer, mesh.indexCount, 1, 0, 0, 0);
    }
}

void RenderBackend::setUniformBufferData(const UniformBufferHandle buffer, const void* data, const size_t size) {
    fillBuffer(m_uniformBuffers[buffer.index], data, size);
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
    assert(m_renderPasses.isGraphicPassHandle(passHandle));
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
    assert(!m_renderPasses.isGraphicPassHandle(passHandle));
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

void RenderBackend::renderFrame(bool presentToScreen) {

    prepareRenderPasses();

    //reset doesn't work before waiting for render finished fence
    resetTimestampQueryPool();

    //record command buffer
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;

    const auto currentCommandBuffer = m_commandBuffers[m_currentCommandBufferIndex];
    m_currentCommandBufferIndex = (m_currentCommandBufferIndex + 1) % 2;

    auto res = vkResetCommandBuffer(currentCommandBuffer, 0);
    assert(res == VK_SUCCESS);
    res = vkBeginCommandBuffer(currentCommandBuffer, &beginInfo);
    assert(res == VK_SUCCESS);

    //index needed for end query
    const uint32_t frameQueryIndex = (uint32_t)m_timestampQueries.size();
    {
        TimestampQuery frameQuery;
        frameQuery.name = "Frame";
        frameQuery.startQuery = issueTimestampQuery(currentCommandBuffer);

        m_timestampQueries.push_back(frameQuery);
    }
    
    
    for (const auto& execution : m_renderPassInternalExecutions) {
        submitRenderPass(execution, currentCommandBuffer);
    }

    //imgui    
    {
        startDebugLabel(currentCommandBuffer, "ImGui");
    
        TimestampQuery imguiQuery;
        imguiQuery.name = "ImGui";
        imguiQuery.startQuery = issueTimestampQuery(currentCommandBuffer);
    
        ImGui::Render();
    
        vkCmdPipelineBarrier(currentCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0, 0, nullptr, 0, nullptr, 1, m_ui.barriers.data());
    
        vkCmdBeginRenderPass(currentCommandBuffer, &m_ui.passBeginInfos[m_swapchainInputImageIndex], VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), currentCommandBuffer);
        vkCmdEndRenderPass(currentCommandBuffer);
    
        imguiQuery.endQuery = issueTimestampQuery(currentCommandBuffer);
        m_timestampQueries.push_back(imguiQuery);
    
        endDebugLabel(currentCommandBuffer);
    }
    
    m_timestampQueries[frameQueryIndex].endQuery = issueTimestampQuery(currentCommandBuffer);
    

    /*
    transition swapchain image to present
    */
    auto& swapchainPresentImage = m_images[m_swapchainInputImageHandle.index];
    const auto& transitionToPresentBarrier = createImageBarriers(swapchainPresentImage, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0, 1);
    barriersCommand(currentCommandBuffer, transitionToPresentBarrier, std::vector<VkBufferMemoryBarrier> {});

    res = vkEndCommandBuffer(currentCommandBuffer);
    assert(res == VK_SUCCESS);

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

    //get timestamp results
    {
        m_renderpassTimings.clear();

        std::vector<uint32_t> timestamps;
        timestamps.resize(m_currentTimestampQueryCount);

        //res = vkGetQueryPoolResults(vkContext.device, m_timestampQueryPool, 0, m_currentTimestampQueryCount,
        //    timestamps.size() * sizeof(uint32_t), timestamps.data(), 0, VK_QUERY_RESULT_WAIT_BIT);
        //assert(res == VK_SUCCESS);
        //on Ryzen 4700U iGPU vkGetQueryPoolResults only returns correct results for the first query
        //maybe it contains more info so needs more space per query?
        //manually get every query for now
        //FIXME: proper solution
        for (size_t i = 0; i < m_currentTimestampQueryCount; i++) {
            auto result = vkGetQueryPoolResults(vkContext.device, m_timestampQueryPool, (uint32_t)i, 1,
                (uint32_t)timestamps.size() * sizeof(uint32_t), &timestamps[i], 0, VK_QUERY_RESULT_WAIT_BIT);
            checkVulkanResult(result);
        }

        for (const auto query : m_timestampQueries) {

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

std::vector<MeshHandle> RenderBackend::createMeshes(const std::vector<MeshBinary>& meshes, const std::vector<Material>& materials) {
    assert(meshes.size() == materials.size());

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

		//textures
        const Material& material = materials[i];

		mesh.materialTextureIndices.albedo = m_images[material.diffuseTexture.index].globalDescriptorSetIndex;
		mesh.materialTextureIndices.normal = m_images[material.normalTexture.index].globalDescriptorSetIndex;
		mesh.materialTextureIndices.specular = m_images[material.specularTexture.index].globalDescriptorSetIndex;

        //store and return handle
        MeshHandle handle = { (uint32_t)m_meshes.size() };
        handles.push_back(handle);
        m_meshes.push_back(mesh);
    }
    return handles;
}

std::vector<DynamicMeshHandle> RenderBackend::createDynamicMeshes(const std::vector<uint32_t>& maxPositionsPerMesh, 
    const std::vector<uint32_t>& maxIndicesPerMesh) {
    if (maxPositionsPerMesh.size() != maxIndicesPerMesh.size()) {
        std::cout << "Warning: RenderBackend::createDynamicMeshes, maxPosition and maxIndices vector sizes do not match\n";
    }

    std::vector<DynamicMeshHandle> handles;
    for (uint32_t i = 0; i < std::min(maxPositionsPerMesh.size(), maxIndicesPerMesh.size()); i++) {
        const auto& maxPositions = maxPositionsPerMesh[i];
        const auto& maxIndices = maxIndicesPerMesh[i];
        handles.push_back(createDynamicMeshInternal(maxPositions, maxIndices));
    }
    return handles;
}

void RenderBackend::updateDynamicMeshes(const std::vector<DynamicMeshHandle>& handles, 
    const std::vector<std::vector<glm::vec3>>& positionsPerMesh, 
    const std::vector<std::vector<uint32_t>>&  indicesPerMesh) {

    if (handles.size() != positionsPerMesh.size() && handles.size() != indicesPerMesh.size()) {
        std::cout << "Warning: RenderBackend::updateDynamicMeshes handle, position and index vector sizes do not match\n";
    };

    const uint32_t meshCount = (uint32_t)std::min(std::min(handles.size(), positionsPerMesh.size()), indicesPerMesh.size());
    for (uint32_t i = 0; i < meshCount; i++) {
        const auto handle = handles[i];
        const auto& positions = positionsPerMesh[i];
        const auto& indices = indicesPerMesh[i];
        auto& mesh = m_dynamicMeshes[handle.index];

        mesh.indexCount = (uint32_t)indices.size();

        //validate position count
        const uint32_t floatPerPosition = 3; //xyz
        const uint32_t maxPositionCount = (uint32_t)mesh.vertexBuffer.size / (sizeof(float) * floatPerPosition);

        if (positions.size() > maxPositionCount) {
            std::cout << "Warning: RenderBackend::updateDynamicMeshes position count exceeds allocated vertex buffer size\n";
        }

        //validate index count
        const uint32_t maxIndexCount = (uint32_t)mesh.indexBuffer.size / sizeof(uint32_t);
        if (indices.size() > maxIndexCount) {
            std::cout << "Warning: RenderBackend::updateDynamicMeshes index count exceeds allocated index buffer size\n";
            mesh.indexCount = maxIndexCount;
        }

        //update buffers
        //there memory is host visible so it can be mapped and copied
        const VkDeviceSize vertexCopySize = std::min(positions.size(), (size_t)maxPositionCount) * sizeof(float) * floatPerPosition;
        fillHostVisibleCoherentBuffer(mesh.vertexBuffer, (char*)positions.data(), vertexCopySize);

        //index count has already been validated
        const VkDeviceSize indexCopySize = mesh.indexCount * sizeof(uint32_t);
        fillHostVisibleCoherentBuffer(mesh.indexBuffer, (char*)indices.data(), indexCopySize);
    }
}

ImageHandle RenderBackend::createImage(const ImageDescription& desc) {

	const VkFormat format = imageFormatToVulkanFormat(desc.format);
	const VkImageAspectFlagBits aspectFlag = imageFormatToVkAspectFlagBits(desc.format);

	VkImageType type;
	VkImageViewType viewType;
	switch (desc.type) {
	case ImageType::Type1D:     type = VK_IMAGE_TYPE_1D; viewType = VK_IMAGE_VIEW_TYPE_1D; break;
	case ImageType::Type2D:     type = VK_IMAGE_TYPE_2D; viewType = VK_IMAGE_VIEW_TYPE_2D; break;
	case ImageType::Type3D:     type = VK_IMAGE_TYPE_3D; viewType = VK_IMAGE_VIEW_TYPE_3D; break;
	case ImageType::TypeCube:   type = VK_IMAGE_TYPE_2D; viewType = VK_IMAGE_VIEW_TYPE_CUBE; break;
	default: throw std::runtime_error("Unsuported type enum");
	}

	uint32_t mipCount;
	switch (desc.mipCount) {
	case(MipCount::One): mipCount = 1; break;
	case(MipCount::Manual): mipCount = desc.manualMipCount; break;
	case(MipCount::FullChain): mipCount = mipCountFromResolution(desc.width, desc.height, desc.depth); break;
	default: throw std::runtime_error("Unsuported mipCoun enum");
	}

	VkImageUsageFlags usage = 0;
	if (bool(desc.usageFlags & ImageUsageFlags::Attachment)) {
		const VkImageUsageFlagBits attachmentUsage = isDepthFormat(format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		usage |= attachmentUsage;
	}
	if (bool(desc.usageFlags & ImageUsageFlags::Sampled)) {
		usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	if (bool(desc.usageFlags & ImageUsageFlags::Storage)) {
		usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}
	if (desc.initialData.size() > 0) {
		usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	if (desc.autoCreateMips) {
		usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	Image image;
	image.extent.width = desc.width;
	image.extent.height = desc.height;
	image.extent.depth = desc.depth;
	image.desc = desc;
	image.format = format;
	image.type = desc.type;

	for (uint32_t i = 0; i < mipCount; i++) {
		image.layoutPerMip.push_back(VK_IMAGE_LAYOUT_UNDEFINED);
	}

	VkImageCreateFlags flags = 0;
	uint32_t arrayLayers = 1;
	if (desc.type == ImageType::TypeCube) {
		flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		arrayLayers = 6;
		assert(desc.width == desc.height);
		assert(desc.depth == 1);
	}

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = nullptr;
	imageInfo.flags = flags;
	imageInfo.imageType = type;
	imageInfo.format = format;
	imageInfo.extent = image.extent;
	imageInfo.mipLevels = mipCount;
	imageInfo.arrayLayers = arrayLayers;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.queueFamilyIndexCount = 1;
	imageInfo.pQueueFamilyIndices = &vkContext.queueFamilies.graphicsQueueIndex;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	auto res = vkCreateImage(vkContext.device, &imageInfo, nullptr, &image.vulkanHandle);
	checkVulkanResult(res);

	//bind memory
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(vkContext.device, image.vulkanHandle, &memoryRequirements);
	const VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	if (!m_vkAllocator.allocate(memoryRequirements, memoryFlags, &image.memory)) {
		throw("Could not allocate image memory");
	}
	res = vkBindImageMemory(vkContext.device, image.vulkanHandle, image.memory.vkMemory, image.memory.offset);
	assert(res == VK_SUCCESS);

	//create image view
	image.viewPerMip.reserve(mipCount);
	for (uint32_t i = 0; i < mipCount; i++) {
		const auto view = createImageView(image, viewType, i, mipCount - i, aspectFlag);
		image.viewPerMip.push_back(view);
	}

	//fill with data
	if (desc.initialData.size() != 0) {
		transferDataIntoImage(image, desc.initialData.data(), desc.initialData.size());
	}

	//generate mipmaps
	if (desc.autoCreateMips) {
		generateMipChain(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	//most textures with sampled usage are used by the material system
	//the material systems assumes the read_only_optimal layout
	//if no mips are generated the layout will still be transfer_dst or undefined
	//to avoid issues all sampled images without mip generation are manually transitioned to read_only_optimal
	if (bool(desc.usageFlags & ImageUsageFlags::Sampled) && !desc.autoCreateMips) {
		const auto transitionCmdBuffer = beginOneTimeUseCommandBuffer();

		const auto newLayoutBarriers = createImageBarriers(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT, 0, (uint32_t)image.viewPerMip.size());
		barriersCommand(transitionCmdBuffer, newLayoutBarriers, std::vector<VkBufferMemoryBarrier> {});

		//end recording
		res = vkEndCommandBuffer(transitionCmdBuffer);
		assert(res == VK_SUCCESS);

		//submit
		VkFence fence = submitOneTimeUseCmdBuffer(transitionCmdBuffer, vkContext.transferQueue);

		res = vkWaitForFences(vkContext.device, 1, &fence, VK_TRUE, UINT64_MAX);
		assert(res == VK_SUCCESS);

		//cleanup
		vkDestroyFence(vkContext.device, fence, nullptr);
		vkFreeCommandBuffers(vkContext.device, m_transientCommandPool, 1, &transitionCmdBuffer);
	}

	//add to global texture descriptor array, if image can be sampled
	if (bool(desc.usageFlags & ImageUsageFlags::Sampled)) {
		if (m_globalTextureArrayDescriptorSetFreeTextureIndices.size() > 0) {
			image.globalDescriptorSetIndex = m_globalTextureArrayDescriptorSetFreeTextureIndices.back();
			m_globalTextureArrayDescriptorSetFreeTextureIndices.pop_back();
		}
		else {
			image.globalDescriptorSetIndex = m_globalTextureArrayDescriptorSetTextureCount;
			m_globalTextureArrayDescriptorSetTextureCount++;
		}

		setGlobalTextureArrayDescriptorSetTexture(image.viewPerMip[0], image.globalDescriptorSetIndex);
	}
	
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

FramebufferHandle RenderBackend::createFramebuffer(const FramebufferDescription& desc) {
    FramebufferHandle handle;
    handle.index = (uint32_t)m_framebuffers.size();

    const auto& pass = m_renderPasses.getGraphicPassRefByHandle(desc.compatibleRenderpass);

    if (!validateFramebufferTargetGraphicPassCombination(desc.targets, desc.compatibleRenderpass)) {
        throw("framebuffer and renderpass are not compatible");
    }

    Framebuffer framebuffer;
    framebuffer.desc = desc;
    framebuffer.vkHandle = createVulkanFramebuffer(desc.targets, pass.vulkanRenderPass);

    m_framebuffers.push_back(framebuffer);

    return handle;
}

ImageHandle RenderBackend::getSwapchainInputImage() {
    auto result = vkAcquireNextImageKHR(vkContext.device, m_swapchain.vulkanHandle, UINT64_MAX, m_swapchain.imageAvaible, VK_NULL_HANDLE, &m_swapchainInputImageIndex);
    checkVulkanResult(result);
    m_swapchainInputImageHandle = m_swapchain.imageHandles[m_swapchainInputImageIndex];
    return m_swapchainInputImageHandle;
}

void RenderBackend::getMemoryStats(uint64_t* outAllocatedSize, uint64_t* outUsedSize) {
    assert(outAllocatedSize != nullptr);
    assert(outUsedSize != nullptr);
    m_vkAllocator.getMemoryStats(outAllocatedSize, outUsedSize);
    *outAllocatedSize   += (uint32_t)m_stagingBufferSize;
    *outUsedSize        += (uint32_t)m_stagingBufferSize;
}

std::vector<RenderPassTime> RenderBackend::getRenderpassTimings() {
    return m_renderpassTimings;
}

void RenderBackend::prepareRenderPasses() {
    
    m_renderPassInternalExecutions.clear();
    auto renderPassesToAdd = m_renderPassExecutions;
    m_renderPassExecutions.clear();

    //order passes
    //iterate over passes, add them if possible
    //adding is possible if all parents have already been added
    //index is reset if pass is added to recheck condition for previous passes
    uint32_t passIndex = 0;
    while (passIndex < renderPassesToAdd.size()) {
        const auto pass = renderPassesToAdd[passIndex];
        bool parentsAvaible = true;
        for (const auto parent : pass.parents) {
            bool parentFound = false;
            for (uint32_t j = 0; j < m_renderPassInternalExecutions.size(); j++) {
                if (m_renderPassInternalExecutions[j].handle.index == parent.index) {
                    parentFound = true;
                    break;
                }
            }
            if (!parentFound) {
                parentsAvaible = false;
                passIndex++;
                break;
            }
        }
        if (parentsAvaible) {
            m_renderPassExecutions.push_back(pass);
            RenderPassExecutionInternal internalExec;
            internalExec.handle = pass.handle;
            internalExec.framebuffer = pass.framebuffer;
            internalExec.dispatches[0] = pass.dispatchCount[0];
            internalExec.dispatches[1] = pass.dispatchCount[1];
            internalExec.dispatches[2] = pass.dispatchCount[2];
            m_renderPassInternalExecutions.push_back(internalExec);
            
            //remove pass by swapping with end
            renderPassesToAdd[passIndex] = renderPassesToAdd.back();
            renderPassesToAdd.pop_back();
            passIndex = 0;
        }
    }
    assert(renderPassesToAdd.size() == 0); //all passes must have been added

    
    //create barriers
    for (uint32_t i = 0; i < m_renderPassExecutions.size(); i++) {

        auto& execution = m_renderPassExecutions[i];
        std::vector<VkImageMemoryBarrier> barriers;
        const auto& resources = execution.resources;

        //storage images        
        for (auto& storageImage : resources.storageImages) {
            Image& image = m_images[storageImage.image.index];

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
            for (const auto& barrier : barriers) {
                if (barrier.image == image.vulkanHandle) {
                    hasBarrierAlready = true;
                    break;
                }
            }

            if ((image.currentlyWriting || needsLayoutTransition) && !hasBarrierAlready) {
                const auto& layoutBarriers = createImageBarriers(image, requiredLayout,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, 0, (uint32_t)image.layoutPerMip.size());
                barriers.insert(barriers.end(), layoutBarriers.begin(), layoutBarriers.end());
            }
            image.currentlyWriting = true;
        }

        //sampled images        
        for (auto& sampledImage : resources.sampledImages) {

            //use general layout if image is used as a storage image too
            bool isUsedAsStorageImage = false;
            {
                for (auto& storageImage : resources.storageImages) {
                    if (storageImage.image.index == sampledImage.image.index) {
                        isUsedAsStorageImage = true;
                        break;
                    }
                }
            }
            if (isUsedAsStorageImage) {
                continue;
            }

            Image& image = m_images[sampledImage.image.index];

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
                barriers.insert(barriers.end(), layoutBarriers.begin(), layoutBarriers.end());
            }
        }

        //attachments        
        if (m_renderPasses.isGraphicPassHandle(execution.handle)) {
            const auto& framebuffer = m_framebuffers[execution.framebuffer.index];
            for (const auto target : framebuffer.desc.targets) {
                Image& image = m_images[target.image.index];

                //check if any mip levels need a layout transition                
                const VkImageLayout requiredLayout = isDepthFormat(image.format) ?
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                bool needsLayoutTransition = false;
                for (const auto& layout : image.layoutPerMip) {
                    if (layout != requiredLayout) {
                        needsLayoutTransition = true;
                    }
                }

                if (image.currentlyWriting || needsLayoutTransition) {
                    const VkAccessFlags access = isDepthFormat(image.format) ?
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                    const auto& layoutBarriers = createImageBarriers(image, requiredLayout, access, 0, 
                        (uint32_t)image.viewPerMip.size());
                    barriers.insert(barriers.end(), layoutBarriers.begin(), layoutBarriers.end());
                }
                image.currentlyWriting = true;
            }
        }
        
        m_renderPassInternalExecutions[i].imageBarriers = barriers;

        //storage buffer barriers
        for (const auto& bufferResource : resources.storageBuffers) {
            StorageBufferHandle handle = bufferResource.buffer;
            Buffer& buffer = m_storageBuffers[handle.index];
            const bool needsBarrier = buffer.isBeingWritten;
            if (needsBarrier) {
                VkBufferMemoryBarrier barrier = createBufferBarrier(buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
                m_renderPassInternalExecutions[i].memoryBarriers.push_back(barrier);
            }

            //update writing state
            buffer.isBeingWritten = !bufferResource.readOnly;
        }
    }

    /*
    add UI barriers
    */
    m_ui.barriers = createImageBarriers(m_images[m_swapchainInputImageHandle.index], 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, 1);
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

void RenderBackend::submitRenderPass(const RenderPassExecutionInternal& execution, const VkCommandBuffer commandBuffer) {

    TimestampQuery timeQuery;

    if (m_renderPasses.isGraphicPassHandle(execution.handle)) {

        auto& pass = m_renderPasses.getGraphicPassRefByHandle(execution.handle);
        startDebugLabel(commandBuffer, pass.graphicPassDesc.name);

        timeQuery.name = pass.graphicPassDesc.name;
        timeQuery.startQuery = issueTimestampQuery(commandBuffer);

        barriersCommand(commandBuffer, execution.imageBarriers, execution.memoryBarriers);

        const Framebuffer framebuffer = m_framebuffers[execution.framebuffer.index];
        const glm::ivec2 resolution = resolutionFromFramebufferTargets(framebuffer.desc.targets);
        const auto beginInfo = createBeginInfo(resolution.x, resolution.y, pass.vulkanRenderPass, 
            framebuffer.vkHandle, pass.clearValues);

        //prepare pass
        vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

        //stop recording mesh commands
        vkEndCommandBuffer(pass.meshCommandBuffer);

        //execute mesh commands
        vkCmdExecuteCommands(commandBuffer, 1, &pass.meshCommandBuffer);

        vkCmdEndRenderPass(commandBuffer);
    }
    else {
        auto& pass = m_renderPasses.getComputePassRefByHandle(execution.handle);
        startDebugLabel(commandBuffer, pass.computePassDesc.name);

        timeQuery.name = pass.computePassDesc.name;
        timeQuery.startQuery = issueTimestampQuery(commandBuffer);

        barriersCommand(commandBuffer, execution.imageBarriers, execution.memoryBarriers);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipeline);
        VkDescriptorSet sets[3] = { m_globalDescriptorSet, pass.descriptorSet };
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipelineLayout, 0, 2, sets, 0, nullptr);
        vkCmdDispatch(commandBuffer, execution.dispatches[0], execution.dispatches[1], execution.dispatches[2]);
    }

    timeQuery.endQuery = issueTimestampQuery(commandBuffer);
    endDebugLabel(commandBuffer);
    m_timestampQueries.push_back(timeQuery);
}

void RenderBackend::waitForRenderFinished() {
    auto result = vkWaitForFences(vkContext.device, 1, &m_renderFinishedFence, VK_TRUE, INT64_MAX);
    checkVulkanResult(result);
}

bool RenderBackend::validateFramebufferTargetGraphicPassCombination(const std::vector<FramebufferTarget>& targets,
    const RenderPassHandle graphicPassHandle) {

    const std::string failureMessagePrologue = "Validation failed of framebuffertarget/renderpass combination: ";
    if (!m_renderPasses.isGraphicPassHandle(graphicPassHandle)) {
        std::cout << failureMessagePrologue << "renderpass is not a graphic pass\n";
        return false;
    }
    const GraphicPass pass = m_renderPasses.getGraphicPassRefByHandle(graphicPassHandle);

    const auto& renderpassAttachments = pass.graphicPassDesc.attachments;

    if (targets.size() != renderpassAttachments.size()) {
        std::cout << failureMessagePrologue << "attachment and target count mismatch\n";
        return false;
    }
    for (int i = 0; i < targets.size(); i++) {
        const FramebufferTarget& target = targets[i];
        const Attachment& attachment = renderpassAttachments[i];
        const Image& targetImage = m_images[target.image.index];
        if (imageFormatToVulkanFormat(attachment.format) != targetImage.format) {
            std::cout << failureMessagePrologue << "attachment and target image format mismatch\n";
            return false;
        }
    }
    return true;
}

std::vector<const char*> RenderBackend::getRequiredInstanceExtensions() {

    //query required glfw extensions
    uint32_t requiredExtensionGlfwCount = 0;
    const char** requiredExtensionsGlfw = glfwGetRequiredInstanceExtensions(&requiredExtensionGlfwCount);

    //add debug extension if used
    std::vector<const char*> requestedExtensions(requiredExtensionsGlfw, requiredExtensionsGlfw + requiredExtensionGlfwCount);
    requestedExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (m_useValidationLayers) {
        requestedExtensions.push_back("VK_EXT_debug_report");
    }

    return requestedExtensions;
}

void RenderBackend::createVulkanInstance() {

    //retrieve and print requested extensions
    std::vector<const char*> requestedExtensions = getRequiredInstanceExtensions();
    std::cout << "requested extensions: " << std::endl;
    for (const auto ext : requestedExtensions) {
        std::cout << ext << std::endl;
    }
    std::cout << std::endl;

    //list avaible extensions
    uint32_t avaibleExtensionCount = 0;
    auto res = vkEnumerateInstanceExtensionProperties(nullptr, &avaibleExtensionCount, nullptr);
    assert(res == VK_SUCCESS);
    std::vector<VkExtensionProperties> avaibleExtensions(avaibleExtensionCount);
    res = vkEnumerateInstanceExtensionProperties(nullptr, &avaibleExtensionCount, avaibleExtensions.data());
    assert(res == VK_SUCCESS);

    std::cout << "avaible instance extensions: " << std::endl;
    for (const auto& ext : avaibleExtensions) {
        std::cout << ext.extensionName << std::endl;
    }
    std::cout << std::endl;

    //ensure all required extensions are avaible
    for (size_t i = 0; i < requestedExtensions.size(); i++) {
        std::string required = requestedExtensions[i];
        bool supported = false;
        for (const auto& avaible : avaibleExtensions) {
            if (avaible.extensionName == required) {
                supported = true;
                break;
            }
        }
        if (!supported) {
            throw std::runtime_error("required instance extension not avaible: " + required);
        }
    }

    //list avaible layers
    uint32_t avaibleLayerCount = 0;
    res = vkEnumerateInstanceLayerProperties(&avaibleLayerCount, nullptr);
    assert(res == VK_SUCCESS);
    std::vector<VkLayerProperties> avaibleLayers(avaibleLayerCount);
    res = vkEnumerateInstanceLayerProperties(&avaibleLayerCount, avaibleLayers.data());
    assert(res == VK_SUCCESS);

    std::cout << "avaible layers" << std::endl;
    for (const auto& avaible : avaibleLayers) {
        std::cout << avaible.layerName << std::endl;
    }
    std::cout << std::endl;

    //validation layers
    std::vector<const char*> requestedLayers;
    if (m_useValidationLayers) {
        requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
    }

    //ensure all requested layers are avaible
    for (const auto& requested : requestedLayers) {
        bool isAvaible = false;
        std::string requestedName(requested);
        for (const auto& avaible : avaibleLayers) {
            if (requestedName == avaible.layerName) {
                isAvaible = true;
                break;
            }
        }
        if (!isAvaible) {
            throw std::runtime_error("requested layer not avaible: " + requestedName);
        }
    }

    //create instance
    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pNext = nullptr;
    instanceInfo.flags = 0;

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "Plain Renderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_MAKE_VERSION(1, 2, 0);

    instanceInfo.pApplicationInfo = &appInfo;

    instanceInfo.enabledLayerCount = (uint32_t)requestedLayers.size();
    instanceInfo.ppEnabledLayerNames = requestedLayers.data();

    instanceInfo.enabledExtensionCount = (uint32_t)requestedExtensions.size();
    instanceInfo.ppEnabledExtensionNames = requestedExtensions.data();

    res = vkCreateInstance(&instanceInfo, nullptr, &vkContext.vulkanInstance);
    checkVulkanResult(res);
}

bool RenderBackend::hasRequiredDeviceFeatures(const VkPhysicalDevice physicalDevice) {
	//check features
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);

    VkPhysicalDeviceVulkan12Features features12;
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = nullptr;

    VkPhysicalDeviceFeatures2 features2;
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features12;

    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

	const bool supportsRequiredFeatures = 
		features.samplerAnisotropy &&
		features.imageCubeArray &&
		features.fragmentStoresAndAtomics &&
		features.fillModeNonSolid &&
		features.depthClamp &&
		features.geometryShader &&
		features12.hostQueryReset &&
		features12.runtimeDescriptorArray &&
		features12.descriptorBindingPartiallyBound &&
		features12.descriptorBindingVariableDescriptorCount;

	//check device extensions
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());

	bool supportsDeviceExtensions = false;

	//currently only requiring conservative rasterisation
	for (const VkExtensionProperties& ext : extensions) {
		if (strcmp(ext.extensionName, VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME)) {
			supportsDeviceExtensions = true;
			break;
		}
	}

	return supportsRequiredFeatures && supportsDeviceExtensions;
        
}

void RenderBackend::pickPhysicalDevice() {

    //enumerate devices
    uint32_t deviceCount = 0;
    auto res = vkEnumeratePhysicalDevices(vkContext.vulkanInstance, &deviceCount, nullptr);
    assert(res == VK_SUCCESS);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    res = vkEnumeratePhysicalDevices(vkContext.vulkanInstance, &deviceCount, devices.data());
    assert(res == VK_SUCCESS);

    //pick first suitable device
    for (const auto& device : devices) {
        QueueFamilies families;
        if (getQueueFamilies(device, &families) && hasRequiredDeviceFeatures(device)) {
            vkContext.physicalDevice = device;
            break;
        }
    }

    if (vkContext.physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find suitable physical device");
    }

    //retrieve and output device name
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(vkContext.physicalDevice, &deviceProperties);
    m_nanosecondsPerTimestamp = deviceProperties.limits.timestampPeriod;
    
    std::cout << "picked physical device: " << deviceProperties.deviceName << std::endl;
    std::cout << std::endl;
}

bool RenderBackend::getQueueFamilies(const VkPhysicalDevice device, QueueFamilies* pOutQueueFamilies) {

    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

    bool foundCompute = false;
    bool foundGraphics = false;
    bool foundPresentation = false;

    //iterate families and check if they fit our needs
    for (uint32_t i = 0; i < familyCount; i++) {
        if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            pOutQueueFamilies->computeQueueIndex = i;
            foundCompute = true;
        }
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            pOutQueueFamilies->graphicsQueueIndex = i;
            pOutQueueFamilies->transferQueueFamilyIndex = i;
            foundGraphics = true;
        }

        VkBool32 isPresentationQueue;
        auto res = vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_swapchain.surface, &isPresentationQueue);
        checkVulkanResult(res);

        if (isPresentationQueue) {
            pOutQueueFamilies->presentationQueueIndex = i;
            foundPresentation = true;
        }
    }
    return foundCompute && foundGraphics && foundPresentation;
}

void RenderBackend::acquireDebugUtilsExtFunctionsPointers() {
    m_debugExtFunctions.vkCmdBeginDebugUtilsLabelEXT    = (PFN_vkCmdBeginDebugUtilsLabelEXT)    vkGetDeviceProcAddr(vkContext.device, "vkCmdBeginDebugUtilsLabelEXT");
    m_debugExtFunctions.vkCmdEndDebugUtilsLabelEXT      = (PFN_vkCmdEndDebugUtilsLabelEXT)      vkGetDeviceProcAddr(vkContext.device, "vkCmdEndDebugUtilsLabelEXT");
    m_debugExtFunctions.vkCmdInsertDebugUtilsLabelEXT   = (PFN_vkCmdInsertDebugUtilsLabelEXT)   vkGetDeviceProcAddr(vkContext.device, "vkCmdInsertDebugUtilsLabelEXT");
    m_debugExtFunctions.vkCreateDebugUtilsMessengerEXT  = (PFN_vkCreateDebugUtilsMessengerEXT)  vkGetDeviceProcAddr(vkContext.device, "vkCreateDebugUtilsMessengerEXT");
    m_debugExtFunctions.vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetDeviceProcAddr(vkContext.device, "vkDestroyDebugUtilsMessengerEXT");
    m_debugExtFunctions.vkQueueBeginDebugUtilsLabelEXT  = (PFN_vkQueueBeginDebugUtilsLabelEXT)  vkGetDeviceProcAddr(vkContext.device, "vkQueueBeginDebugUtilsLabelEXT");
    m_debugExtFunctions.vkQueueEndDebugUtilsLabelEXT    = (PFN_vkQueueEndDebugUtilsLabelEXT)    vkGetDeviceProcAddr(vkContext.device, "vkQueueEndDebugUtilsLabelEXT");
    m_debugExtFunctions.vkQueueInsertDebugUtilsLabelEXT = (PFN_vkQueueInsertDebugUtilsLabelEXT) vkGetDeviceProcAddr(vkContext.device, "vkQueueInsertDebugUtilsLabelEXT");
    m_debugExtFunctions.vkSetDebugUtilsObjectNameEXT    = (PFN_vkSetDebugUtilsObjectNameEXT)    vkGetDeviceProcAddr(vkContext.device, "vkSetDebugUtilsObjectNameEXT");
    m_debugExtFunctions.vkSetDebugUtilsObjectTagEXT     = (PFN_vkSetDebugUtilsObjectTagEXT)     vkGetDeviceProcAddr(vkContext.device, "vkSetDebugUtilsObjectTagEXT");
    m_debugExtFunctions.vkSubmitDebugUtilsMessageEXT    = (PFN_vkSubmitDebugUtilsMessageEXT)    vkGetDeviceProcAddr(vkContext.device, "vkSubmitDebugUtilsMessageEXT");
}

void RenderBackend::createLogicalDevice() {

    //set removes duplicates
    std::set<uint32_t> uniqueQueueFamilies = {
        vkContext.queueFamilies.graphicsQueueIndex,
        vkContext.queueFamilies.computeQueueIndex,
        vkContext.queueFamilies.presentationQueueIndex,
        vkContext.queueFamilies.transferQueueFamilyIndex
    };

    //queue infos
    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    //TODO make queues unique if possible
    for (auto& family : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo info;
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.pNext = nullptr;
        info.flags = 0;
        info.queueCount = 1;
        const float priority = 1.f;
        info.pQueuePriorities = &priority;
        info.queueFamilyIndex = family;
        queueInfos.push_back(info);
    }

    VkPhysicalDeviceFeatures features = {};
    features.samplerAnisotropy = true;
    features.fragmentStoresAndAtomics = true;
    features.fillModeNonSolid = true;
    features.depthClamp = true;
	features.geometryShader = true;

    VkPhysicalDeviceVulkan12Features features12 = {}; //vulkan 1.2 features
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = nullptr;
    features12.hostQueryReset = true;
	features12.runtimeDescriptorArray = true;
	features12.descriptorBindingPartiallyBound = true;
	features12.descriptorBindingVariableDescriptorCount = true;

    //device info
    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &features12;
    deviceInfo.flags = 0;
    deviceInfo.queueCreateInfoCount = (uint32_t)queueInfos.size();
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.enabledLayerCount = 0;			//depreceated and ignored
    deviceInfo.ppEnabledLayerNames = nullptr;	//depreceated and ignored
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.pEnabledFeatures = &features;

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;

    auto res = vkCreateDevice(vkContext.physicalDevice, &deviceInfo, nullptr, &vkContext.device);
    checkVulkanResult(res);
}

VkDebugReportCallbackEXT RenderBackend::setupDebugCallbacks() {

    //callback info
    VkDebugReportCallbackCreateInfoEXT callbackInfo;
    callbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    callbackInfo.pNext = nullptr;
    callbackInfo.flags =
        VK_DEBUG_REPORT_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_ERROR_BIT_EXT;
    callbackInfo.pfnCallback = debugReportCallback;
    callbackInfo.pUserData = nullptr;

    //get entry point
    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>
        (vkGetInstanceProcAddr(vkContext.vulkanInstance, "vkCreateDebugReportCallbackEXT"));

    VkDebugReportCallbackEXT debugCallback;
    auto res = vkCreateDebugReportCallbackEXT(vkContext.vulkanInstance, &callbackInfo, nullptr, &debugCallback);
    checkVulkanResult(res);

    return debugCallback;
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

void RenderBackend::getSwapchainImages(const uint32_t width, const uint32_t height) {

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
        image.extent.width = width;
        image.extent.height = height;
        image.extent.depth = 1;
        image.format = m_swapchain.surfaceFormat.format;
        image.viewPerMip.push_back(createImageView(image, VK_IMAGE_VIEW_TYPE_2D, 0, 1, VK_IMAGE_ASPECT_COLOR_BIT));
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

    //create framebuffers    
    VkExtent2D extent;
    extent.width  = m_images[m_swapchain.imageHandles[0].index].extent.width;
    extent.height = m_images[m_swapchain.imageHandles[0].index].extent.height;
    for (const auto& imageHandle : m_swapchain.imageHandles) {
        FramebufferTarget uiTarget;
        uiTarget.image = imageHandle;
        uiTarget.mipLevel = 0;
        const VkFramebuffer framebuffer = createVulkanFramebuffer({ uiTarget }, m_ui.renderPass);
        m_ui.framebuffers.push_back(framebuffer);
    }

    //pass infos    
    for (const auto& framebuffer : m_ui.framebuffers) {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = m_ui.renderPass;
        info.framebuffer = framebuffer;
        info.renderArea.extent = extent;
        info.clearValueCount = 0;
        info.pClearValues = nullptr;
        m_ui.passBeginInfos.push_back(info);
    }
}

VkImageView RenderBackend::createImageView(const Image image, const VkImageViewType viewType, 
    const uint32_t baseMip, const uint32_t mipLevels, const VkImageAspectFlags aspectMask) {

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.pNext = nullptr;
    viewInfo.flags = 0;
    viewInfo.image = image.vulkanHandle;
    viewInfo.viewType = viewType;
    viewInfo.format = image.format;

    VkComponentMapping components = {};
    components.r = VK_COMPONENT_SWIZZLE_R;
    components.g = VK_COMPONENT_SWIZZLE_G;
    components.b = VK_COMPONENT_SWIZZLE_B;
    components.a = VK_COMPONENT_SWIZZLE_A;

    viewInfo.components = components;
    uint32_t layerCount = 1;
    if (viewType == VK_IMAGE_VIEW_TYPE_CUBE) {
        layerCount = 6;
    }

    VkImageSubresourceRange subresource = {};
    subresource.aspectMask = aspectMask;
    subresource.baseMipLevel = baseMip;
    subresource.levelCount = mipLevels;
    subresource.baseArrayLayer = 0;
    subresource.layerCount = layerCount;

    viewInfo.subresourceRange = subresource;

    VkImageView view;
    auto res = vkCreateImageView(vkContext.device, &viewInfo, nullptr, &view);
    checkVulkanResult(res);

    return view;
}

DynamicMeshHandle RenderBackend::createDynamicMeshInternal(const uint32_t maxPositions, const uint32_t maxIndices) {
    
    DynamicMesh mesh;
    mesh.indexCount = 0;

    std::vector<uint32_t> queueFamilies = { vkContext.queueFamilies.graphicsQueueIndex };
    const uint32_t memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    //create vertex buffer    
    const uint32_t floatsPerPosition = 3; //xyz
    VkDeviceSize vertexDataSize = maxPositions * sizeof(float) * floatsPerPosition;

    mesh.vertexBuffer = createBufferInternal(vertexDataSize, queueFamilies,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, memoryFlags);

    //create index buffer
    VkDeviceSize indexDataSize = maxIndices * sizeof(uint32_t);
    mesh.indexBuffer = createBufferInternal(indexDataSize, queueFamilies, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, memoryFlags);

    //save and return handle
    DynamicMeshHandle handle = { (uint32_t)m_dynamicMeshes.size() };
    m_dynamicMeshes.push_back(mesh);

    return handle;
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
    layers.aspectMask = isDepthFormat(image.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
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
    VkDeviceSize currentMipWidth = target.extent.width;
    VkDeviceSize currentMipHeight = target.extent.height;
    VkDeviceSize currentMipDepth = target.extent.depth;

    VkDeviceSize bytesPerRow    = (size_t)(target.extent.width * bytePerPixel);
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

            //reduce size depending on dimensions
            if (target.type == ImageType::Type1D) {
                currentMipSize /= 2;
            }
            else if (target.type == ImageType::Type2D) {
                currentMipSize /= 4;
            }
            else if (target.type == ImageType::Type3D) {
                currentMipSize /= 8;
            }
            else {
                std::cout << "Error: unknown image type" << std::endl;
                assert(false);
                return;
            }

            //memory offset per mip is reset
            mipMemoryOffset = 0;

            //BCn compressed textures store at least a 4x4 pixel block, resulting in at least a 4 pixel row
            if (isBCnCompressed) {
                const VkDeviceSize minPixelPerRox = 4;
                bytesPerRow = std::max(bytesPerRow, VkDeviceSize(minPixelPerRox * bytePerPixel));

                const VkDeviceSize minPixelsPerMip = 16;
                currentMipSize = std::max(currentMipSize, VkDeviceSize(minPixelsPerMip * bytePerPixel));
            }
        }

        
        //the size to copy is limited either by
        //-the staging buffer size
        //-the size left to copy on the current mip level
        VkDeviceSize copySize = std::min(m_stagingBufferSize, currentMipSize - mipMemoryOffset);

        //always copy entire rows
        copySize = copySize / bytesPerRow * bytesPerRow;

        //copy data to staging buffer
        fillHostVisibleCoherentBuffer(m_stagingBuffer, (char*)data + totalMemoryOffset, copySize);

        //begin command buffer for copying
        VkCommandBuffer copyBuffer = beginOneTimeUseCommandBuffer();

        //layout transition to transfer_dst the first time
        if (totalMemoryOffset == 0) {
            const auto toTransferDstBarrier = createImageBarriers(target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_TRANSFER_READ_BIT, 0, (uint32_t)target.viewPerMip.size());

            barriersCommand(copyBuffer, toTransferDstBarrier, std::vector<VkBufferMemoryBarrier> {});
        }
        

        //calculate which region to copy
        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.imageSubresource = createSubresourceLayers(target, mipLevel);
        //entire rows are copied, so the starting row(offset.y) is the current mip offset divided by the row size
        region.imageOffset = { 0, int32_t(mipMemoryOffset / bytesPerRow), 0 };
        region.bufferRowLength = (uint32_t)currentMipWidth;
        region.bufferImageHeight = (uint32_t)currentMipHeight;
        //copy as many rows as fit into the copy size, without going over the mip height
        region.imageExtent.height = (uint32_t)std::min(copySize / bytesPerRow, currentMipHeight);
        region.imageExtent.width = (uint32_t)currentMipWidth;
        region.imageExtent.depth = (uint32_t)currentMipDepth;

        //BCn compressed textures are stored in 4x4 pixel blocks, so that is the minimum buffer size
        if (isBCnCompressed) {
            region.bufferRowLength      = std::max(region.bufferRowLength,      (uint32_t)4);;
            region.bufferImageHeight    = std::max(region.bufferImageHeight,    (uint32_t)4);;
        }

        //issue for commands, then wait
        vkCmdCopyBufferToImage(copyBuffer, m_stagingBuffer.vulkanHandle, target.vulkanHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        vkEndCommandBuffer(copyBuffer);
        VkFence fence = submitOneTimeUseCmdBuffer(copyBuffer, vkContext.transferQueue);
        auto result = vkWaitForFences(vkContext.device, 1, &fence, VK_TRUE, UINT64_MAX);
        checkVulkanResult(result);

        //cleanup
        vkDestroyFence(vkContext.device, fence, nullptr);
        vkFreeCommandBuffers(vkContext.device, m_transientCommandPool, 1, &copyBuffer);

        //update memory offsets
        //BCn compressed textures store at least a 4x4 pixel block
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

    /*
    check for linear filtering support
    */
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(vkContext.physicalDevice, image.format, &formatProps);
    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("physical device lacks linear filtering support for used format");
    }

    VkImageBlit blitInfo;

    /*
    offset base stays zero
    */
    blitInfo.srcOffsets[0].x = 0;
    blitInfo.srcOffsets[0].y = 0;
    blitInfo.srcOffsets[0].z = 0;

    blitInfo.dstOffsets[0].x = 0;
    blitInfo.dstOffsets[0].y = 0;
    blitInfo.dstOffsets[0].z = 0;

    /*
    initial offset extent
    */
    blitInfo.srcOffsets[1].x = image.extent.width;
    blitInfo.srcOffsets[1].y = image.extent.height;
    blitInfo.srcOffsets[1].z = image.extent.depth;

    blitInfo.dstOffsets[1].x = blitInfo.srcOffsets[1].x != 1 ? blitInfo.srcOffsets[1].x / 2 : 1;
    blitInfo.dstOffsets[1].y = blitInfo.srcOffsets[1].y != 1 ? blitInfo.srcOffsets[1].y / 2 : 1;
    blitInfo.dstOffsets[1].z = blitInfo.srcOffsets[1].z != 1 ? blitInfo.srcOffsets[1].z / 2 : 1;

    const auto blitCmdBuffer = beginOneTimeUseCommandBuffer();

    for (uint32_t srcMip = 0; srcMip < image.viewPerMip.size() - 1; srcMip++) {

        /*
        barriers
        */
        auto barriers = createImageBarriers(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, srcMip, 1); //src
        const auto dstBarriers = createImageBarriers(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, srcMip + 1, 1); //dst
        barriers.insert(barriers.end(), dstBarriers.begin(), dstBarriers.end());

        barriersCommand(blitCmdBuffer, barriers, std::vector<VkBufferMemoryBarrier> {});

        /*
        blit operation
        */
        blitInfo.srcSubresource = createSubresourceLayers(image, srcMip );
        blitInfo.dstSubresource = createSubresourceLayers(image, srcMip + 1);

        vkCmdBlitImage(blitCmdBuffer, image.vulkanHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image.vulkanHandle, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blitInfo, VK_FILTER_LINEAR);

        /*
        update offsets
        */
        blitInfo.srcOffsets[1].x /= blitInfo.srcOffsets[1].x != 1 ? 2 : 1;
        blitInfo.srcOffsets[1].y /= blitInfo.srcOffsets[1].y != 1 ? 2 : 1;
        blitInfo.srcOffsets[1].z /= blitInfo.srcOffsets[1].z != 1 ? 2 : 1;

        blitInfo.dstOffsets[1].x = blitInfo.srcOffsets[1].x != 1 ? blitInfo.srcOffsets[1].x / 2 : 1;
        blitInfo.dstOffsets[1].y = blitInfo.srcOffsets[1].y != 1 ? blitInfo.srcOffsets[1].y / 2 : 1;
        blitInfo.dstOffsets[1].z = blitInfo.srcOffsets[1].z != 1 ? blitInfo.srcOffsets[1].z / 2 : 1;
    }

    /*
    bring image into new layout
    */
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

VkCommandBuffer RenderBackend::allocateCommandBuffer(const VkCommandBufferLevel level) {

    VkCommandBufferAllocateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.commandPool = m_commandPool;
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
    /*
    buffer and images are given via pointer
    stored in vector to keep pointer valid
    resize first to avoid push_back invalidating pointers
    */
    uint32_t imageInfoIndex = 0;
    uint32_t bufferInfoIndex = 0;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;

    imageInfos.resize(resources.samplers.size() + resources.storageImages.size() + resources.sampledImages.size());
    bufferInfos.resize(resources.uniformBuffers.size() + resources.storageBuffers.size());

    /*
    samplers
    */
    for (const auto& resource : resources.samplers) {
        VkDescriptorImageInfo samplerInfo;
        samplerInfo.sampler = m_samplers[resource.sampler.index];
        imageInfos[imageInfoIndex] = samplerInfo;
        const auto writeSet = createWriteDescriptorSet(resource.binding, VK_DESCRIPTOR_TYPE_SAMPLER, nullptr, &imageInfos[imageInfoIndex]);
        imageInfoIndex++;
        descriptorInfos.push_back(writeSet);
    }

    /*
    sampled images
    */
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

        imageInfo.imageView = m_images[resource.image.index].viewPerMip[resource.mipLevel];
        imageInfos[imageInfoIndex] = imageInfo;
        const auto writeSet = createWriteDescriptorSet(resource.binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 
            nullptr, &imageInfos[imageInfoIndex]);
        imageInfoIndex++;
        descriptorInfos.push_back(writeSet);
    }

    /*
    storage images
    */
    for (const auto& resource : resources.storageImages) {
        VkDescriptorImageInfo imageInfo;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = m_images[resource.image.index].viewPerMip[resource.mipLevel];
        imageInfos[imageInfoIndex] = imageInfo;
        const auto writeSet = createWriteDescriptorSet(resource.binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr, &imageInfos[imageInfoIndex]);
        imageInfoIndex++;
        descriptorInfos.push_back(writeSet);
    }

    /*
    uniform buffer
    */
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

    /*
    storage buffer
    */
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

VkPipelineLayout RenderBackend::createPipelineLayout(const VkDescriptorSetLayout setLayout, const bool isGraphicPass, 
	const VkShaderStageFlags stageFlags) {

    VkPushConstantRange matrices = {};
    matrices.stageFlags = stageFlags;
    matrices.offset = 0;
    matrices.size = 140;

    VkDescriptorSetLayout setLayouts[3] = { m_globalDescriptorSetLayout, setLayout, m_globalTextureArrayDescriporSetLayout };
    uint32_t setCount = isGraphicPass ? 3 : 2;

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.flags = 0;
    layoutInfo.setLayoutCount = setCount;
    layoutInfo.pSetLayouts = setLayouts;
    layoutInfo.pushConstantRangeCount = isGraphicPass ? 1 : 0;
    layoutInfo.pPushConstantRanges = isGraphicPass ? &matrices : nullptr;

    VkPipelineLayout layout = {};
    auto res = vkCreatePipelineLayout(vkContext.device, &layoutInfo, nullptr, &layout);
    checkVulkanResult(res);

    return layout;
}

ComputePass RenderBackend::createComputePassInternal(const ComputePassDescription& desc, const std::vector<uint32_t>& spirV) {

    ComputePass pass;
    pass.computePassDesc = desc;
    VkComputePipelineCreateInfo pipelineInfo;

    VkShaderModule module = createShaderModule(spirV);
    ShaderReflection reflection = performComputeShaderReflection(spirV);
    pass.descriptorSetLayout = createDescriptorSetLayout(reflection.shaderLayout);
    pass.pipelineLayout = createPipelineLayout(pass.descriptorSetLayout, false, 0);

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

    /*
    descriptor set
    */
    const auto setSizes = descriptorSetAllocationSizeFromShaderLayout(reflection.shaderLayout);
    pass.descriptorSet = allocateDescriptorSet(pass.descriptorSetLayout, setSizes);
    assert(pass.descriptorSet != VK_NULL_HANDLE);
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
    pass.meshCommandBuffer = allocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);

    VkShaderModule vertexModule   = createShaderModule(spirV.vertex);
    VkShaderModule fragmentModule = createShaderModule(spirV.fragment);

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

    ShaderReflection reflection = performShaderReflection(spirV);
    pass.descriptorSetLayout = createDescriptorSetLayout(reflection.shaderLayout);
    pass.pipelineLayout = createPipelineLayout(pass.descriptorSetLayout, true, pipelineLayoutStageFlags);

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
    pass.descriptorSet = allocateDescriptorSet(pass.descriptorSetLayout, setSizes);

    return pass;
}

bool validateImageFitForFramebuffer(const Image& image) {
    const bool hasRequiredUsage = bool(image.desc.usageFlags | ImageUsageFlags::Attachment);
    return hasRequiredUsage;
}

bool validateAttachmentFormatsAreCompatible(const ImageFormat a, const ImageFormat b) {
    return imageFormatToVkAspectFlagBits(a) == imageFormatToVkAspectFlagBits(b);
}

bool RenderBackend::validateAttachments(const std::vector<FramebufferTarget>& targets) {

    const std::string failureMessagePrologue = "Attachment validation failed: ";
    if (targets.size() == 0) {
        std::cout << failureMessagePrologue << "no attachments\n";
        return false;
    }

    glm::uvec2 resolution = resolutionFromFramebufferTargets(targets);

    for (const auto attachmentDefinition : targets) {

        const Image attachment = m_images[attachmentDefinition.image.index];

        if (!validateImageFitForFramebuffer(attachment)) {
            std::cout << failureMessagePrologue << "attachment image not fit for framebuffer\n";
            return false;
        }

        //all attachments need same resolution
        if (attachment.extent.width != resolution.x || attachment.extent.height != resolution.y) {
            std::cout << failureMessagePrologue << "image resolutions not matching\n";
            return false;
        }

        //attachment must be 2D
        if (attachment.extent.depth != 1) {
            std::cout << failureMessagePrologue << "image depth not 1, 2D image required for framebuffer\n";
            return false;
        }
    }
    return true;
}

glm::uvec2 RenderBackend::resolutionFromFramebufferTargets(const std::vector<FramebufferTarget>& targets) {
    if (targets.size() == 0) {
        return glm::uvec2(0);
    }
    const Image firstImage = m_images[targets[0].image.index];
    return glm::uvec2(firstImage.extent.width, firstImage.extent.height);
}

VkRenderPass RenderBackend::createVulkanRenderPass(const std::vector<Attachment>& attachments) {

    VkRenderPass            pass;
    VkRenderPassCreateInfo  info;
    std::vector<VkAttachmentDescription> descriptions;
    VkSubpassDescription subpass;
    std::vector<VkAttachmentReference> colorReferences;
    VkAttachmentReference  depthReference;
    bool hasDepth = false;

    /*
    attachment descriptions
    */
    for (uint32_t i = 0; i < attachments.size(); i++) {

        Attachment attachment = attachments[i];
        VkAttachmentLoadOp loadOp;
        VkAttachmentDescription desc;

        switch (attachment.loadOp) {
        case(AttachmentLoadOp::Clear): loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; break;
        case(AttachmentLoadOp::Load): loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; break;
        case(AttachmentLoadOp::DontCare): loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; break;
        default: throw std::runtime_error("Unknown attachment load op");
        }

        VkImageLayout layout = isDepthFormat(attachment.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        desc.flags = 0;
        desc.format = imageFormatToVulkanFormat(attachment.format);
        desc.samples = VK_SAMPLE_COUNT_1_BIT;
        desc.loadOp = loadOp;
        desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc.initialLayout = layout;
        desc.finalLayout = layout;

        VkAttachmentReference ref;
        ref.attachment = i;
        ref.layout = desc.initialLayout;
        if (desc.format == VK_FORMAT_D16_UNORM ||
            desc.format == VK_FORMAT_D32_SFLOAT) {
            depthReference = ref;
            hasDepth = true;
        }
        else {
            colorReferences.push_back(ref);
        }
        descriptions.push_back(desc);
    }

    /*
    subpass
    */
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.colorAttachmentCount = (uint32_t)colorReferences.size();
    subpass.pColorAttachments = colorReferences.data();
    subpass.pResolveAttachments = nullptr;
    subpass.pDepthStencilAttachment = hasDepth ? &depthReference : nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;

    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.attachmentCount = (uint32_t)descriptions.size();
    info.pAttachments = descriptions.data();
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 0;
    info.pDependencies = nullptr;

    auto res = vkCreateRenderPass(vkContext.device, &info, nullptr, &pass);
    checkVulkanResult(res);

    return pass;
}

VkFramebuffer RenderBackend::createVulkanFramebuffer(const std::vector<FramebufferTarget>& targets, 
    const VkRenderPass compatibleRenderpass) {

    if (!validateAttachments(targets)) {
        std::cout << "createVulkanFramebuffer: invalid attachments\n";
        return VK_NULL_HANDLE;
    }

    std::vector<VkImageView> views;
    for (const auto& target : targets) {
        const auto image = m_images[target.image.index];
        const auto view = image.viewPerMip[target.mipLevel];
        views.push_back(view);
    }

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.pNext = nullptr;
    framebufferInfo.flags = 0;
    framebufferInfo.renderPass = compatibleRenderpass;
    framebufferInfo.attachmentCount = (uint32_t)views.size();
    framebufferInfo.pAttachments = views.data();

    const glm::ivec2 resolution = resolutionFromFramebufferTargets(targets);
    framebufferInfo.width = resolution.x;
    framebufferInfo.height = resolution.y;
    framebufferInfo.layers = 1;

    VkFramebuffer framebuffer;
    auto res = vkCreateFramebuffer(vkContext.device, &framebufferInfo, nullptr, &framebuffer);
    checkVulkanResult(res);

    return framebuffer;
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

    //no stencil used, infos don't matter
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

bool RenderBackend::isDepthFormat(ImageFormat format) {
    return (format == ImageFormat::Depth16 ||
        format == ImageFormat::Depth32);
}

bool RenderBackend::isDepthFormat(VkFormat format) {
    return (
        format == VK_FORMAT_D16_UNORM ||
        format == VK_FORMAT_D16_UNORM_S8_UINT ||
        format == VK_FORMAT_D24_UNORM_S8_UINT ||
        format == VK_FORMAT_D32_SFLOAT ||
        format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

void RenderBackend::barriersCommand(const VkCommandBuffer commandBuffer, 
    const std::vector<VkImageMemoryBarrier>& imageBarriers, const std::vector<VkBufferMemoryBarrier>& memoryBarriers) {

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 
        (uint32_t)memoryBarriers.size(), memoryBarriers.data(), (uint32_t)imageBarriers.size(), imageBarriers.data());
}

std::vector<VkImageMemoryBarrier> RenderBackend::createImageBarriers(Image& image, const VkImageLayout newLayout,
    const VkAccessFlags dstAccess, const uint32_t baseMip, const uint32_t mipLevels) {

    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    const bool isDepthImage = isDepthFormat(image.format);
    if (isDepthImage) {
        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    std::vector<VkImageMemoryBarrier> barriers;

    uint32_t layerCount = 1;
    if (image.type == ImageType::TypeCube) {
        layerCount = 6;
    }

    /*
    first barrier
    */ 
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
    
    /*
    add subsequent mip level barriers
    */
    for (uint32_t i = 1; i < mipLevels; i++) {
        /*
        same mip layout: extens subresource range
        */
        uint32_t mipLevel = baseMip + i;
        if (image.layoutPerMip[mipLevel] == barriers.back().oldLayout) {
            barriers.back().subresourceRange.levelCount++;
        }
        /*
        different mip layout: new barrier
        */
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

    /*
    update image properties
    */
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
    createInfo.pipelineStatistics = 0; //pipeline queries not handled for now

    VkQueryPool pool;
    const auto res = vkCreateQueryPool(vkContext.device, &createInfo, nullptr, &pool);
    assert(res == VK_SUCCESS);

    vkResetQueryPool(vkContext.device, pool, 0, queryCount);

    return pool;
}

void RenderBackend::resetTimestampQueryPool() {
    m_timestampQueries.resize(0);
    vkResetQueryPool(vkContext.device, m_timestampQueryPool, 0, m_currentTimestampQueryCount);
    m_currentTimestampQueryCount = 0;
}

uint32_t RenderBackend::issueTimestampQuery(const VkCommandBuffer cmdBuffer) {
    const uint32_t query = m_currentTimestampQueryCount;
    vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_timestampQueryPool, query);
    m_currentTimestampQueryCount++;
    return query;
}

//in release mode the function is empty, resulting in a warning
//disable warning for the function
#pragma warning( push )
#pragma warning( disable : 4100 ) //C4100: unreferenced formal parameter

void RenderBackend::checkVulkanResult(const VkResult result) {
#ifndef NDEBUG
    if (result != VK_SUCCESS) {
        std::cout << "Vulkan function failed\n";
    }
#endif 
}

//reenable warning
#pragma warning( pop )

void RenderBackend::destroyImage(const ImageHandle handle) {

    m_freeImageHandles.push_back(handle);

    const Image image = m_images[handle.index];

	if (bool(image.desc.usageFlags & ImageUsageFlags::Sampled)) {
		m_globalTextureArrayDescriptorSetFreeTextureIndices.push_back(image.globalDescriptorSetIndex);
	}
	
    for (const auto& view : image.viewPerMip) {
        vkDestroyImageView(vkContext.device, view, nullptr);
    }
    
    /*
    swapchain images have no manualy allocated memory
    they are deleted by the swapchain
    view has to be destroyed manually though
    */
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

void RenderBackend::destroyDynamicMesh(const DynamicMesh& mesh) {
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

void RenderBackend::destroyFramebuffer(const Framebuffer& framebuffer) {
    vkDestroyFramebuffer(vkContext.device, framebuffer.vkHandle, nullptr);
}