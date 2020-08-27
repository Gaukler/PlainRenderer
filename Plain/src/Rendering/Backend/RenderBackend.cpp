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
#include "TypeConversion.h"

#include <imgui/imgui.h>
#include <imgui/examples/imgui_impl_glfw.h>
#include <imgui/examples/imgui_impl_vulkan.h>

/*
==================

RenderPasses

==================
*/

/*
=========
isGraphicPassHandle
=========
*/
bool RenderPasses::isGraphicPassHandle(const RenderPassHandle handle) {
    //checks first bit
    const uint32_t upperBit = 1 << 31;
    return handle.index & upperBit;
}

/*
=========
addGraphicPass
=========
*/
RenderPassHandle RenderPasses::addGraphicPass(const GraphicPass pass) {
    uint32_t index = m_graphicPasses.size();
    m_graphicPasses.push_back(pass);
    return indexToGraphicPassHandle(index);
}

/*
=========
addComputePass
=========
*/
RenderPassHandle RenderPasses::addComputePass(const ComputePass pass) {
    uint32_t index = m_computePasses.size();
    m_computePasses.push_back(pass);
    return indexToComputePassHandle(index);
}

/*
=========
getNGraphicPasses
=========
*/
uint32_t RenderPasses::getNGraphicPasses() {
    return m_graphicPasses.size();
}

/*
=========
getNComputePasses
=========
*/
uint32_t RenderPasses::getNComputePasses(){
    return m_computePasses.size();
}

/*
=========
getGraphicPassReferenceByHandle
=========
*/
GraphicPass& RenderPasses::getGraphicPassRefByHandle(const RenderPassHandle handle) {
    assert(isGraphicPassHandle(handle));
    return m_graphicPasses[graphicPassHandleToIndex(handle)];
}

/*
=========
getComputePassReferenceByHandle
=========
*/
ComputePass& RenderPasses::getComputePassRefByHandle(const RenderPassHandle handle) {
    assert(!isGraphicPassHandle(handle));
    return m_computePasses[computePassHandleToIndex(handle)];
}

/*
=========
getGraphicPassReferenceByIndex
=========
*/
GraphicPass& RenderPasses::getGraphicPassRefByIndex(const uint32_t index) {
    return m_graphicPasses[index];
}

/*
=========
getComputePassReferenceByIndex
=========
*/
ComputePass& RenderPasses::getComputePassRefByIndex(const uint32_t index) {
    return m_computePasses[index];
}

/*
=========
updateGraphicPassByHandle
=========
*/
void RenderPasses::updateGraphicPassByHandle(const GraphicPass pass, const RenderPassHandle handle) {
    const uint32_t index = graphicPassHandleToIndex(handle);
    updateGraphicPassByIndex(pass, index);
}

/*
=========
updateComputePassByHandle
=========
*/
void RenderPasses::updateComputePassByHandle(const ComputePass pass, const RenderPassHandle handle) {
    const uint32_t index = computePassHandleToIndex(handle);
    updateComputePassByIndex(pass, index);
}

/*
=========
updateGraphicPassByIndex
=========
*/
void RenderPasses::updateGraphicPassByIndex(const GraphicPass pass, const uint32_t index) {
    m_graphicPasses[index] = pass;
}

/*
=========
updateComputePassByIndex
=========
*/
void RenderPasses::updateComputePassByIndex(const ComputePass pass, const uint32_t index) {
    m_computePasses[index] = pass;
}

/*
=========
graphicPassHandleToIndex
=========
*/
uint32_t RenderPasses::graphicPassHandleToIndex(const RenderPassHandle handle) {
    //set first bit to 0
    const uint32_t noUpperBit = ~(1 << 31);
    return handle.index & noUpperBit;
}

/*
=========
computePassHandleToIndex
=========
*/
uint32_t RenderPasses::computePassHandleToIndex(const RenderPassHandle handle) {
    //first bit already 0, just return
    return handle.index;
}

/*
=========
indexToGraphicPassHandle
=========
*/
RenderPassHandle RenderPasses::indexToGraphicPassHandle(const uint32_t index) {
    //set first bit to 1 and cast
    const uint32_t upperBit = 1 << 31;
    return { index | upperBit };
}

/*
=========
indexToComputePassHandle
=========
*/
RenderPassHandle RenderPasses::indexToComputePassHandle(const uint32_t index) {
    //first bit should already be 0, just cast
    return { index };
}

/*
==================

RenderBackend

==================
*/


/*
=========
debugReportCallback
=========
*/
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

/*
==================

public functions

==================
*/

/*
=========
setup
=========
*/
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
        m_stagingBuffer = createBufferInternal(m_stagingBufferSize, stagingBufferQueueFamilies, stagingBufferUsageFlags, stagingBufferMemoryFlags);
    }
    
    /*
    create common descriptor set layouts
    */
    ShaderLayout globalLayout;
    globalLayout.uniformBufferBindings.push_back(0);
    m_globalDescriptorSetLayout = createDescriptorSetLayout(globalLayout);

    m_commandBuffers[0] = allocateCommandBuffer();
    m_commandBuffers[1] = allocateCommandBuffer();

    /*
    create global storage buffer
    */
    std::vector<uint32_t> queueFamilies = { vkContext.queueFamilies.graphicsQueueIndex, vkContext.queueFamilies.computeQueueIndex };
    GlobalShaderInfo defaultInfo;
    UniformBufferDescription globalShaderBufferDesc;
    globalShaderBufferDesc.size = sizeof(GlobalShaderInfo);
    globalShaderBufferDesc.initialData = &defaultInfo;
    m_globalShaderInfoBuffer = createUniformBuffer(globalShaderBufferDesc);

    /*
    create global info descriptor set
    */
    {
        RenderPassResources globalResources;
        UniformBufferResource globalBufferResource(m_globalShaderInfoBuffer, true, 0);
        globalResources.uniformBuffers.push_back(globalBufferResource);

        DescriptorPoolAllocationSizes globalDescriptorSetSizes;
        globalDescriptorSetSizes.uniformBuffer = 1;

        m_globalDescriptorSet = allocateDescriptorSet(m_globalDescriptorSetLayout, globalDescriptorSetSizes);
        updateDescriptorSet(m_globalDescriptorSet, globalResources);
    }

    m_materialSamplers = createMaterialSamplers();

    /*
    imgui
    */
    setupImgui(window);

    //query pools
    m_timestampQueryPool = createQueryPool(VK_QUERY_TYPE_TIMESTAMP, m_timestampQueryPoolQueryCount);
}

/*
=========
shutdown
=========
*/
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

/*
=========
recreateSwapchain
=========
*/
void RenderBackend::recreateSwapchain(const uint32_t width, const uint32_t height, GLFWwindow* window) {

    vkDeviceWaitIdle(vkContext.device);

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

        const auto attachment = Attachment(
            m_swapchain.imageHandles[i], 
            0, 
            0, 
            AttachmentLoadOp::Load);

        VkFramebuffer newBuffer = createFramebuffer(m_ui.renderPass, extent, std::vector<Attachment> { attachment });
        m_ui.framebuffers[i] = newBuffer;
        m_ui.passBeginInfos[i].framebuffer = newBuffer;
        m_ui.passBeginInfos[i].renderArea.extent = extent;
    }
}

/*
=========
reloadShaders
=========
*/
void RenderBackend::updateShaderCode() {

    //helper comparing last change dates of cache and source and updating date to prevent constant reloading if shader is not compiling
    auto isShaderOutOfDate = [](std::filesystem::path relativePath) {
        const auto absolutePath = absoluteShaderPathFromRelative(relativePath);
        const auto cachePath = shaderCachePathFromRelative(relativePath);
        assert(std::filesystem::exists(absolutePath));
        assert(std::filesystem::exists(cachePath));
        return std::filesystem::last_write_time(absolutePath) > std::filesystem::last_write_time(cachePath);
    };

    //iterate over all render passes and check for every shader if it's out of date
    //graphic passes
    std::vector<uint32_t> outOfDateGraphicPasses;
    for (uint32_t i = 0; i < m_renderPasses.getNGraphicPasses(); i++) {
        bool needsUpdate = false;
        const auto shaderDescriptions = m_renderPasses.getGraphicPassRefByIndex(i).graphicPassDesc.shaderDescriptions;
        needsUpdate |= isShaderOutOfDate(shaderDescriptions.vertex.srcPathRelative);
        needsUpdate |= isShaderOutOfDate(shaderDescriptions.fragment.srcPathRelative);
        if (shaderDescriptions.geometry.has_value()) {
            needsUpdate |= isShaderOutOfDate(shaderDescriptions.geometry.value().srcPathRelative);
        }
        if (shaderDescriptions.tesselationControl.has_value()) {
            needsUpdate |= isShaderOutOfDate(shaderDescriptions.tesselationControl.value().srcPathRelative);
        }
        if (shaderDescriptions.tesselationEvaluation.has_value()) {
            needsUpdate |= isShaderOutOfDate(shaderDescriptions.tesselationEvaluation.value().srcPathRelative);
        }           
        if (needsUpdate) {
            outOfDateGraphicPasses.push_back(i);
        }
    }

    //compute passes
    std::vector<uint32_t> outOfDateComputePasses;
    for (uint32_t i = 0; i < m_renderPasses.getNComputePasses(); i++) {
        if (isShaderOutOfDate(m_renderPasses.getComputePassRefByIndex(i).computePassDesc.shaderDescription.srcPathRelative)) {
            outOfDateComputePasses.push_back(i);
        }
    }

    //return if no updates are needed
    if (outOfDateGraphicPasses.size() + outOfDateComputePasses.size()  == 0) {
        return;
    }

    //when updating passes they must not be used
    vkDeviceWaitIdle(vkContext.device);

    /*
    iterate over all out of date passes
    if a shader can't be loaded or compiled it's just skipped as the current version can still be used
    */
    for (const auto passIndex : outOfDateGraphicPasses) {
        GraphicPassShaderSpirV spirV;
        auto& pass = m_renderPasses.getGraphicPassRefByIndex(passIndex);
        if (loadGraphicPassShaders(pass.graphicPassDesc.shaderDescriptions, &spirV)) {
            destroyGraphicPass(pass);
            pass = createGraphicPassInternal(pass.graphicPassDesc, spirV);
        }
    }

    for (const auto passIndex : outOfDateComputePasses) {
        std::vector<uint32_t> spirV;
        auto& pass = m_renderPasses.getComputePassRefByIndex(passIndex);
        if (loadShader(pass.computePassDesc.shaderDescription, &spirV)) {
            destroyComputePass(pass);
            pass = createComputePassInternal(pass.computePassDesc, spirV);
        }
    }
}

/*
=========
resizeImages
=========
*/
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

    /*
    recreate framebuffer that use image
    */
    VkExtent2D extent;
    extent.width = width;
    extent.height = height;

    VkRect2D rect = {};
    rect.extent = extent;
    rect.offset = { 0, 0 };

    for (uint32_t i = 0; i < m_renderPasses.getNGraphicPasses(); i++) {
        bool mustBeResized = false;
        auto pass = m_renderPasses.getGraphicPassRefByIndex(i);
        for (const auto& image : images) {
            //search for image
            for (const auto handle : pass.attachments) {
                if (handle.index == image.index) {
                    mustBeResized = true;
                    break;
                }
            }
        }
        if (mustBeResized) {
            vkDestroyFramebuffer(vkContext.device, pass.beginInfo.framebuffer, nullptr);
            pass.beginInfo.framebuffer = createFramebuffer(pass.vulkanRenderPass, extent, pass.attachmentDescriptions);
            pass.beginInfo.renderArea = rect;
            pass.viewport.width = width;
            pass.viewport.height = height;
            pass.scissor.extent = extent;
            m_renderPasses.updateGraphicPassByIndex(pass, i);
        }
    }
}

/*
=========
newFrame
=========
*/
void RenderBackend::newFrame() {
    m_renderPassExecutions.clear();

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

/*
=========
setRenderPassExecution
=========
*/
void RenderBackend::setRenderPassExecution(const RenderPassExecution& execution) {
    m_renderPassExecutions.push_back(execution);
}

/*
=========
drawMeshes
=========
*/
void RenderBackend::drawMeshes(const std::vector<MeshHandle> meshHandles, 
    const std::vector<std::array<glm::mat4, 2>>& primarySecondaryMatrices, const RenderPassHandle passHandle) {
    if (meshHandles.size() != primarySecondaryMatrices.size()) {
        std::cout << "Error: drawMeshes handle and matrix count does not match\n";
    }
    for (uint32_t i = 0; i < std::min(meshHandles.size(), primarySecondaryMatrices.size()); i++) {

        const auto meshHandle = meshHandles[i];
        const auto mesh = m_meshes[meshHandle.index];
        MeshRenderCommand command;
        command.indexBuffer = mesh.indexBuffer.vulkanHandle;
        command.indexPrecision = mesh.indexPrecision;
        command.indexCount = mesh.indexCount;
        command.primaryMatrix = primarySecondaryMatrices[i][0];
        command.secondaryMatrix = primarySecondaryMatrices[i][1];

        assert(m_renderPasses.isGraphicPassHandle(passHandle));
        auto& pass = m_renderPasses.getGraphicPassRefByHandle(passHandle);
        for (const auto& material : mesh.materials) {
            if (material.flags == pass.materialFeatures) {
                command.materialSet = material.descriptorSet;
            }
        }
        for (const auto& vertexBuffer : mesh.vertexBuffers) {
            if (vertexBuffer.flags == pass.vertexInputFlags) {
                command.vertexBuffer = vertexBuffer.buffer.vulkanHandle;
            }
        }
        pass.meshRenderCommands.push_back(command);
    }
}

/*
=========
drawDynamicMeshes
=========
*/
void RenderBackend::drawDynamicMeshes(const std::vector<DynamicMeshHandle> meshHandles,
    const std::vector<std::array<glm::mat4, 2>>& primarySecondaryMatrices, const RenderPassHandle passHandle) {
    if (meshHandles.size() != primarySecondaryMatrices.size()) {
        std::cout << "Error: drawMeshes handle and modelMatrix count does not match\n";
    }
    for (uint32_t i = 0; i < std::min(meshHandles.size(), primarySecondaryMatrices.size()); i++) {

        const auto meshHandle = meshHandles[i];
        const auto mesh = m_dynamicMeshes[meshHandle.index];
        DynamicMeshRenderCommand command;
        command.indexCount = mesh.indexCount;
        command.vertexBuffer = mesh.vertexBuffer.buffer.vulkanHandle;
        command.indexBuffer = mesh.indexBuffer.vulkanHandle;
        command.primaryMatrix = primarySecondaryMatrices[i][0];
        command.secondaryMatrix = primarySecondaryMatrices[i][1];

        assert(m_renderPasses.isGraphicPassHandle(passHandle));
        auto& pass = m_renderPasses.getGraphicPassRefByHandle(passHandle);
        pass.dynamicMeshRenderCommands.push_back(command);
    }
}

/*
=========
setGlobalShaderInfo
=========
*/
void RenderBackend::setGlobalShaderInfo(const GlobalShaderInfo& info) {
    const auto buffer = m_uniformBuffers[m_globalShaderInfoBuffer.index];
    fillBuffer(buffer, &info, sizeof(info));
}

/*
=========
updateGraphicPassShaderDescription
=========
*/
void RenderBackend::updateGraphicPassShaderDescription(const RenderPassHandle passHandle, const GraphicPassShaderDescriptions& desc) {
    assert(m_renderPasses.isGraphicPassHandle(passHandle));
    auto& pass = m_renderPasses.getGraphicPassRefByHandle(passHandle);
    pass.graphicPassDesc.shaderDescriptions = desc;
    GraphicPassShaderSpirV spirV;
    if (loadGraphicPassShaders(pass.graphicPassDesc.shaderDescriptions, &spirV)) {
        vkDeviceWaitIdle(vkContext.device);
        destroyGraphicPass(pass);
        pass = createGraphicPassInternal(pass.graphicPassDesc, spirV);
    }
}

void RenderBackend::updateComputePassShaderDescription(const RenderPassHandle passHandle, const ShaderDescription& desc) {
    assert(!m_renderPasses.isGraphicPassHandle(passHandle));
    auto& pass = m_renderPasses.getComputePassRefByHandle(passHandle);
    pass.computePassDesc.shaderDescription = desc;
    std::vector<uint32_t> spirV;
    if (loadShader(pass.computePassDesc.shaderDescription, &spirV)) {
        vkDeviceWaitIdle(vkContext.device);
        destroyComputePass(pass);
        pass = createComputePassInternal(pass.computePassDesc, spirV);
    }
}

/*
=========
renderFrame
=========
*/
void RenderBackend::renderFrame() {

    prepareRenderPasses();

    //wait for previous frame to render so resources are avaible
    vkWaitForFences(vkContext.device, 1, &m_renderFinishedFence, VK_TRUE, UINT64_MAX);
    vkResetFences(vkContext.device, 1, &m_renderFinishedFence);

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

    vkResetCommandBuffer(currentCommandBuffer, 0);
    vkBeginCommandBuffer(currentCommandBuffer, &beginInfo);

    //index needed for end query
    const uint32_t frameQueryIndex = m_timestampQueries.size();
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

    vkEndCommandBuffer(currentCommandBuffer);

    //submit 
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = nullptr;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &m_swapchain.imageAvaible;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit.pWaitDstStageMask = &waitStage;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &currentCommandBuffer;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &m_renderFinishedSemaphore;

    vkQueueSubmit(vkContext.graphicQueue, 1, &submit, m_renderFinishedFence);

    presentImage(m_renderFinishedSemaphore);
    glfwPollEvents();

    //get timestamp results
    {
        m_renderpassTimings.clear();

        std::vector<uint32_t> timestamps;
        timestamps.resize(m_currentTimestampQueryCount);
        vkGetQueryPoolResults(vkContext.device, m_timestampQueryPool, 0, m_currentTimestampQueryCount,
            timestamps.size() * sizeof(uint32_t), timestamps.data(), 0, VK_QUERY_RESULT_WAIT_BIT);

        for (const auto query : m_timestampQueries) {

            const float startTime = timestamps[query.startQuery];
            const float endTime = timestamps[query.endQuery];
            const float time = endTime - startTime;

            const float nanoseconds = time * m_nanosecondsPerTimestamp;
            const float milliseconds = nanoseconds * 0.000001f;

            RenderPassTime timing;
            timing.name = query.name;
            timing.timeMs = milliseconds;
            m_renderpassTimings.push_back(timing);
        }
    }
    

    /*
    cleanup
    */
    for (uint32_t i = 0; i < m_renderPasses.getNGraphicPasses(); i++) {
        m_renderPasses.getGraphicPassRefByIndex(i).meshRenderCommands.clear();
        m_renderPasses.getGraphicPassRefByIndex(i).dynamicMeshRenderCommands.clear();
    }
}

/*
==================

create resources

==================
*/

/*
=========
createComputePass
=========
*/
RenderPassHandle RenderBackend::createComputePass(const ComputePassDescription& desc) {

    std::vector<uint32_t> spirV;
    if (!loadShader(desc.shaderDescription, &spirV)) {
        std::cout << "Initial shader loading failed" << std::endl; //loadShaders provides error details trough cout
        throw;
    }

    ComputePass pass = createComputePassInternal(desc, spirV);
    return m_renderPasses.addComputePass(pass);
}

/*
=========
createGraphicPass
=========
*/
RenderPassHandle RenderBackend::createGraphicPass(const GraphicPassDescription& desc) {

    GraphicPassShaderSpirV spirV;
    if (!loadGraphicPassShaders(desc.shaderDescriptions, &spirV)) {
        std::cout << "Initial shader loading failed" << std::endl;
        throw;
    }

    GraphicPass pass = createGraphicPassInternal(desc, spirV);    
    return m_renderPasses.addGraphicPass(pass);;
}

/*
=========
createMeshes
=========
*/
std::vector<MeshHandle> RenderBackend::createMeshes(const std::vector<MeshDataInternal>& meshes, const std::vector<RenderPassHandle>& passes) {
    std::vector<MeshHandle> handles;
    for (const auto& data : meshes) {
        handles.push_back(createMeshInternal(data, passes));
    }
    return handles;
}

/*
=========
createDynamicMeshes
=========
*/
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

/*
=========
updateDynamicMeshes
=========
*/
void RenderBackend::updateDynamicMeshes(const std::vector<DynamicMeshHandle>& handles, 
    const std::vector<std::vector<glm::vec3>>& positionsPerMesh, 
    const std::vector<std::vector<uint32_t>>&  indicesPerMesh) {

    if (handles.size() != positionsPerMesh.size() && handles.size() != indicesPerMesh.size()) {
        std::cout << "Warning: RenderBackend::updateDynamicMeshes handle, position and index vector sizes do not match\n";
    };

    const uint32_t meshCount = std::min(std::min(handles.size(), positionsPerMesh.size()), indicesPerMesh.size());
    for (uint32_t i = 0; i < meshCount; i++) {
        const auto handle = handles[i];
        const auto& positions = positionsPerMesh[i];
        const auto& indices = indicesPerMesh[i];
        auto& mesh = m_dynamicMeshes[handle.index];

        mesh.indexCount = indices.size();

        //validate position count
        const uint32_t floatPerPosition = 3; //xyz
        const uint32_t maxPositionCount = mesh.vertexBuffer.buffer.size / (sizeof(float) * floatPerPosition);

        if (positions.size() > maxPositionCount) {
            std::cout << "Warning: RenderBackend::updateDynamicMeshes position count exceeds allocated vertex buffer size\n";
        }

        //validate index count
        const uint32_t maxIndexCount = mesh.indexBuffer.size / sizeof(uint32_t);
        if (indices.size() > maxIndexCount) {
            std::cout << "Warning: RenderBackend::updateDynamicMeshes index count exceeds allocated index buffer size\n";
            mesh.indexCount = maxIndexCount;
        }

        //update buffers
        //there memory is host visible so it can be mapped and copied
        const VkDeviceSize vertexCopySize = std::min(positions.size(), (size_t)maxPositionCount) * sizeof(float) * floatPerPosition;
        fillHostVisibleCoherentBuffer(mesh.vertexBuffer.buffer, (char*)positions.data(), vertexCopySize);

        //index count has already been validated
        const VkDeviceSize indexCopySize = mesh.indexCount * sizeof(uint32_t);
        fillHostVisibleCoherentBuffer(mesh.indexBuffer, (char*)indices.data(), indexCopySize);
    }
}

/*
=========
createImage
=========
*/
ImageHandle RenderBackend::createImage(const ImageDescription& desc) {

    VkFormat format;
    VkImageAspectFlagBits aspectFlag;
    switch (desc.format) {
    case ImageFormat::R8:               format = VK_FORMAT_R8_UNORM;                aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    case ImageFormat::RG8:              format = VK_FORMAT_R8G8_UNORM;              aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    case ImageFormat::RGBA8:            format = VK_FORMAT_R8G8B8A8_UNORM;          aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    case ImageFormat::RG16_sFloat:      format = VK_FORMAT_R16G16_SFLOAT;           aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    case ImageFormat::RG32_sFloat:      format = VK_FORMAT_R32G32_SFLOAT;           aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    case ImageFormat::RGBA16_sFloat:    format = VK_FORMAT_R16G16B16A16_SFLOAT;     aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    case ImageFormat::R11G11B10_uFloat: format = VK_FORMAT_B10G11R11_UFLOAT_PACK32; aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    case ImageFormat::RGBA32_sFloat:    format = VK_FORMAT_R32G32B32A32_SFLOAT;     aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    case ImageFormat::Depth16:          format = VK_FORMAT_D16_UNORM;               aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT; break;
    case ImageFormat::Depth32:          format = VK_FORMAT_D32_SFLOAT;              aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT; break;
    case ImageFormat::BC1:              format = VK_FORMAT_BC1_RGB_UNORM_BLOCK;     aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    case ImageFormat::BC3:              format = VK_FORMAT_BC3_UNORM_BLOCK;         aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    case ImageFormat::BC5:              format = VK_FORMAT_BC5_UNORM_BLOCK;         aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    default: throw std::runtime_error("missing format defintion");
    }

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
    if (desc.usageFlags & ImageUsageFlags::IMAGE_USAGE_ATTACHMENT) {
        const VkImageUsageFlagBits attachmentUsage = isDepthFormat(format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        usage |= attachmentUsage;
    }
    if (desc.usageFlags & ImageUsageFlags::IMAGE_USAGE_SAMPLED) {
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (desc.usageFlags & ImageUsageFlags::IMAGE_USAGE_STORAGE) {
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
    assert(res == VK_SUCCESS);

    //bind memory
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(vkContext.device, image.vulkanHandle, &memoryRequirements);
    const VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (!m_vkAllocator.allocate(memoryRequirements, memoryFlags, &image.memory)) {
        throw("Could not allocate image memory");
    }
    vkBindImageMemory(vkContext.device, image.vulkanHandle, image.memory.vkMemory, image.memory.offset);

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

    /*
    most textures with sampled usage are used by the material system
    the material systems assumes the read_only_optimal layout
    if no mips are generated the layout will still be transfer_dst or undefined
    to avoid issues all sampled images without mip generation are manually transitioned to read_only_optimal
    */
    if ((desc.usageFlags & ImageUsageFlags::IMAGE_USAGE_SAMPLED) && !desc.autoCreateMips) {
        const auto transitionCmdBuffer = beginOneTimeUseCommandBuffer();

        const auto newLayoutBarriers = createImageBarriers(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            VK_ACCESS_TRANSFER_WRITE_BIT, 0, image.viewPerMip.size());
        barriersCommand(transitionCmdBuffer, newLayoutBarriers, std::vector<VkBufferMemoryBarrier> {});

        //end recording
        vkEndCommandBuffer(transitionCmdBuffer);

        //submit
        VkFence fence = submitOneTimeUseCmdBuffer(transitionCmdBuffer, vkContext.transferQueue);

        vkWaitForFences(vkContext.device, 1, &fence, VK_TRUE, UINT64_MAX);

        //cleanup
        vkDestroyFence(vkContext.device, fence, nullptr);
        vkFreeCommandBuffers(vkContext.device, m_transientCommandPool, 1, &transitionCmdBuffer);
    }
    
    //reuse a free image handle or create a new one
    ImageHandle handle;
    if (m_freeImageHandles.size() > 0) {
        handle = m_freeImageHandles.back();
        m_freeImageHandles.pop_back();
        m_images[handle.index] = image;
    }
    else {
        handle.index = m_images.size();
        m_images.push_back(image);
    }
    return handle;
}

/*
=========
createUniformBuffer
=========
*/
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

    UniformBufferHandle handle = { m_uniformBuffers.size() };
    m_uniformBuffers.push_back(uniformBuffer);
    return handle;
}

/*
=========
createStorageBuffer
=========
*/
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

    StorageBufferHandle handle = { m_storageBuffers.size() };
    m_storageBuffers.push_back(storageBuffer);
    return handle;
}

/*
=========
createSampler
=========
*/
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
    samplerInfo.maxLod = desc.maxMip;
    samplerInfo.borderColor = borderColor;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    VkSampler sampler;
    const auto res = vkCreateSampler(vkContext.device, &samplerInfo, nullptr, &sampler);
    assert(res == VK_SUCCESS);

    SamplerHandle handle = { m_samplers.size() };
    m_samplers.push_back(sampler);
    return handle;
}

/*
=========
setSwapchainInputImage
=========
*/
ImageHandle RenderBackend::getSwapchainInputImage() {

    vkAcquireNextImageKHR(vkContext.device, m_swapchain.vulkanHandle, UINT64_MAX, m_swapchain.imageAvaible, VK_NULL_HANDLE, &m_swapchainInputImageIndex);
    m_swapchainInputImageHandle = m_swapchain.imageHandles[m_swapchainInputImageIndex];
    return m_swapchainInputImageHandle;
}

/*
=========
getMemoryStats
=========
*/
void RenderBackend::getMemoryStats(uint32_t* outAllocatedSize, uint32_t* outUsedSize) {
    assert(outAllocatedSize != nullptr);
    assert(outUsedSize != nullptr);
    m_vkAllocator.getMemoryStats(outAllocatedSize, outUsedSize);
    *outAllocatedSize   += m_stagingBufferSize;
    *outUsedSize        += m_stagingBufferSize;
}

/*
=========
getRenderpassTimings
=========
*/
std::vector<RenderPassTime> RenderBackend::getRenderpassTimings() {
    return m_renderpassTimings;
}

/*
==================

private functions

==================
*/

/*
=========
createMaterialSamplers
=========
*/
MaterialSamplers RenderBackend::createMaterialSamplers(){

    MaterialSamplers samplers;

    SamplerDescription albedoSamplerDesc;
    albedoSamplerDesc.interpolation = SamplerInterpolation::Linear;
    albedoSamplerDesc.wrapping = SamplerWrapping::Repeat;
    albedoSamplerDesc.maxMip = 20;
    albedoSamplerDesc.useAnisotropy = true;
    samplers.albedoSampler = createSampler(albedoSamplerDesc);

    SamplerDescription normalSamplerDesc;
    normalSamplerDesc.interpolation = SamplerInterpolation::Linear;
    normalSamplerDesc.wrapping = SamplerWrapping::Repeat;
    normalSamplerDesc.maxMip = 20;
    normalSamplerDesc.useAnisotropy = true;
    samplers.normalSampler = createSampler(normalSamplerDesc);

    SamplerDescription specularSamplerDesc;
    specularSamplerDesc.interpolation = SamplerInterpolation::Linear;
    specularSamplerDesc.wrapping = SamplerWrapping::Repeat;
    specularSamplerDesc.maxMip = 20;
    specularSamplerDesc.useAnisotropy = true;
    samplers.specularSampler = createSampler(specularSamplerDesc);

    return samplers;
}

/*
=========
prepareRenderPasses
=========
*/
void RenderBackend::prepareRenderPasses() {

    /*
    update descriptor set
    */
    for (const auto pass : m_renderPassExecutions) {
        if (m_renderPasses.isGraphicPassHandle(pass.handle)) {
            updateDescriptorSet(m_renderPasses.getGraphicPassRefByHandle(pass.handle).descriptorSet, pass.resources);
        }
        else {
            updateDescriptorSet(m_renderPasses.getComputePassRefByHandle(pass.handle).descriptorSet, pass.resources);
        }
    }
    
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

    /*
    create image barriers
    */
    for (uint32_t i = 0; i < m_renderPassExecutions.size(); i++) {

        auto& execution = m_renderPassExecutions[i];
        std::vector<VkImageMemoryBarrier> barriers;
        const auto& resources = execution.resources;

        /*
        storage images
        */
        for (auto& storageImage : resources.storageImages) {
            Image& image = m_images[storageImage.image.index];

            /*
            check if any mip levels need a layout transition
            */
            const VkImageLayout requiredLayout = VK_IMAGE_LAYOUT_GENERAL;
            bool needsLayoutTransition = false;
            for (const auto& layout : image.layoutPerMip) {
                if (layout != requiredLayout) {
                    needsLayoutTransition = true;
                }
            }

            /*
            check if image already has a barrier
            can happen if same image is used as two storage image when accessing different mips
            */
            bool hasBarrierAlready = false;
            for (const auto& barrier : barriers) {
                if (barrier.image == image.vulkanHandle) {
                    hasBarrierAlready = true;
                    break;
                }
            }

            if ((image.currentlyWriting || needsLayoutTransition) && !hasBarrierAlready) {
                const auto& layoutBarriers = createImageBarriers(image, requiredLayout,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, 0, image.layoutPerMip.size());
                barriers.insert(barriers.end(), layoutBarriers.begin(), layoutBarriers.end());
                image.currentlyWriting = true;
            }
        }

        /*
        sampled images
        */
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

            /*
            check if any mip levels need a layout transition
            */
            VkImageLayout requiredLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            bool needsLayoutTransition = false;
            for (const auto& layout : image.layoutPerMip) {
                if (layout != requiredLayout) {
                    needsLayoutTransition = true;
                }
            }

            if (image.currentlyWriting | needsLayoutTransition) {
                const auto& layoutBarriers = createImageBarriers(image, requiredLayout, VK_ACCESS_SHADER_READ_BIT, 
                    0, image.viewPerMip.size());
                barriers.insert(barriers.end(), layoutBarriers.begin(), layoutBarriers.end());
            }
        }

        /*
        attachments
        */
        if (m_renderPasses.isGraphicPassHandle(execution.handle)) {
            const auto& pass = m_renderPasses.getGraphicPassRefByHandle(execution.handle);
            for (const auto imageHandle : pass.attachments) {
                Image& image = m_images[imageHandle.index];

                /*
                check if any mip levels need a layout transition
                */
                const VkImageLayout requiredLayout = isDepthFormat(image.format) ?
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                bool needsLayoutTransition = false;
                for (const auto& layout : image.layoutPerMip) {
                    if (layout != requiredLayout) {
                        needsLayoutTransition = true;
                    }
                }

                if (image.currentlyWriting | needsLayoutTransition) {
                    const VkAccessFlags access = isDepthFormat(image.format) ?
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                    const auto& layoutBarriers = createImageBarriers(image, requiredLayout, access, 0, 
                        image.viewPerMip.size());
                    barriers.insert(barriers.end(), layoutBarriers.begin(), layoutBarriers.end());
                    image.currentlyWriting = true;
                }
            }
        }
        
        m_renderPassInternalExecutions[i].imageBarriers = barriers;
    }

    /*
    add UI barriers
    */
    m_ui.barriers = createImageBarriers(m_images[m_swapchainInputImageHandle.index], 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, 1);
}

/*
=========
submitRenderPass
=========
*/
void RenderBackend::submitRenderPass(const RenderPassExecutionInternal& execution, const VkCommandBuffer commandBuffer) {

    barriersCommand(commandBuffer, execution.imageBarriers, execution.memoryBarriers);

    TimestampQuery timeQuery;

    if (m_renderPasses.isGraphicPassHandle(execution.handle)) {

        auto& pass = m_renderPasses.getGraphicPassRefByHandle(execution.handle);
        startDebugLabel(commandBuffer, pass.graphicPassDesc.name);

        timeQuery.name = pass.graphicPassDesc.name;
        timeQuery.startQuery = issueTimestampQuery(commandBuffer);

        /*
        update pointer: might become invalid if pass vector was changed
        */
        pass.beginInfo.pClearValues = pass.clearValues.data();

        //prepare pass
        vkCmdBeginRenderPass(commandBuffer, &pass.beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipeline);
        vkCmdSetViewport(commandBuffer, 0, 1, &pass.viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &pass.scissor);

        //meshes
        for (const auto& mesh : pass.meshRenderCommands) {

            //vertex/index buffers            
            VkDeviceSize offset[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mesh.vertexBuffer, offset);
            vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer, offset[0], mesh.indexPrecision);

            //update push constants
            glm::mat4 matrices[2] = { mesh.primaryMatrix, mesh.secondaryMatrix };
            vkCmdPushConstants(commandBuffer, pass.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(matrices), &matrices);

            //materials            
            VkDescriptorSet sets[3] = { m_globalDescriptorSet, pass.descriptorSet, mesh.materialSet };
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipelineLayout, 0, 3, sets, 0, nullptr);

            vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, 0, 0, 0);
        }

        //dynamic meshes
        for (const auto& mesh : pass.dynamicMeshRenderCommands) {

            //vertex/index buffers
            VkDeviceSize offset[] = { 0 };
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mesh.vertexBuffer, offset);
            vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer, offset[0], VK_INDEX_TYPE_UINT32);

            //update push constants
            glm::mat4 matrices[2] = { mesh.primaryMatrix, mesh.secondaryMatrix };
            vkCmdPushConstants(commandBuffer, pass.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(matrices), &matrices);

            vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, 0, 0, 0);
        }

        vkCmdEndRenderPass(commandBuffer);     

        timeQuery.endQuery = issueTimestampQuery(commandBuffer);
        endDebugLabel(commandBuffer);
    }
    else {
        auto& pass = m_renderPasses.getComputePassRefByHandle(execution.handle);
        startDebugLabel(commandBuffer, pass.computePassDesc.name);

        timeQuery.name = pass.computePassDesc.name;
        timeQuery.startQuery = issueTimestampQuery(commandBuffer);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipeline);
        VkDescriptorSet sets[3] = { m_globalDescriptorSet, pass.descriptorSet };
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipelineLayout, 0, 2, sets, 0, nullptr);
        vkCmdDispatch(commandBuffer, execution.dispatches[0], execution.dispatches[1], execution.dispatches[2]);

        timeQuery.endQuery = issueTimestampQuery(commandBuffer);
        endDebugLabel(commandBuffer);
    }
    m_timestampQueries.push_back(timeQuery);
}

/*
=========
waitForRenderFinished
=========
*/
void RenderBackend::waitForRenderFinished() {
    vkWaitForFences(vkContext.device, 1, &m_renderFinishedFence, VK_TRUE, INT64_MAX);
}

/*
==================

context

==================
*/

/*
=========
getRequiredExtensions
=========
*/
std::vector<const char*> RenderBackend::getRequiredExtensions() {

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

/*
=========
createVulkanInstance()
=========
*/
void RenderBackend::createVulkanInstance() {

    //retrieve and print requested extensions
    std::vector<const char*> requestedExtensions = getRequiredExtensions();
    std::cout << "requested extensions: " << std::endl;
    for (const auto ext : requestedExtensions) {
        std::cout << ext << std::endl;
    }
    std::cout << std::endl;

    //list avaible extensions
    uint32_t avaibleExtensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &avaibleExtensionCount, nullptr);
    std::vector<VkExtensionProperties> avaibleExtensions(avaibleExtensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &avaibleExtensionCount, avaibleExtensions.data());

    std::cout << "avaible extensions: " << std::endl;
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
            throw std::runtime_error("required extension not avaible: " + required);
        }
    }

    //list avaible layers
    uint32_t avaibleLayerCount = 0;
    vkEnumerateInstanceLayerProperties(&avaibleLayerCount, nullptr);
    std::vector<VkLayerProperties> avaibleLayers(avaibleLayerCount);
    vkEnumerateInstanceLayerProperties(&avaibleLayerCount, avaibleLayers.data());

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

    auto res = vkCreateInstance(&instanceInfo, nullptr, &vkContext.vulkanInstance);
    assert(res == VK_SUCCESS);
}

/*
=========
hasRequiredDeviceFeatures
=========
*/
bool RenderBackend::hasRequiredDeviceFeatures(const VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);

    VkPhysicalDeviceVulkan12Features features12;
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = nullptr;

    VkPhysicalDeviceFeatures2 features2;
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features12;

    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

    return
        features.samplerAnisotropy &&
        features.imageCubeArray &&
        features.fragmentStoresAndAtomics &&
        features.fillModeNonSolid &&
        features.depthClamp && 
        features12.hostQueryReset;
}

/*
=========
pickPhysicalDevice
=========
*/
void RenderBackend::pickPhysicalDevice() {

    //enumerate devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vkContext.vulkanInstance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(vkContext.vulkanInstance, &deviceCount, devices.data());

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

/*
=========
getQueueFamilies
=========
*/
bool RenderBackend::getQueueFamilies(const VkPhysicalDevice device, QueueFamilies* pOutQueueFamilies) {

    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

    bool foundCompute = false;
    bool foundGraphics = false;
    bool foundPresentation = false;

    //iterate families and check if they fit our needs
    for (size_t i = 0; i < familyCount; i++) {
        if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            pOutQueueFamilies->computeQueueIndex = (uint32_t)i;
            foundCompute = true;
        }
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            pOutQueueFamilies->graphicsQueueIndex = (uint32_t)i;
            pOutQueueFamilies->transferQueueFamilyIndex = (uint32_t)i;
            foundGraphics = true;
        }

        VkBool32 isPresentationQueue;
        auto res = vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_swapchain.surface, &isPresentationQueue);
        assert(res == VK_SUCCESS);

        if (isPresentationQueue) {
            pOutQueueFamilies->presentationQueueIndex = (uint32_t)i;
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


/*
=========
createLogicalDevice
=========
*/
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

    VkPhysicalDeviceVulkan12Features features12 = {}; //vulkan 1.2 features
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = nullptr;
    features12.hostQueryReset = true;

    //device info
    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &features12;
    deviceInfo.flags = 0;
    deviceInfo.queueCreateInfoCount = queueInfos.size();
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.enabledLayerCount = 0;			//depreceated and ignored
    deviceInfo.ppEnabledLayerNames = nullptr;	//depreceated and ignored
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.pEnabledFeatures = &features;

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;

    auto res = vkCreateDevice(vkContext.physicalDevice, &deviceInfo, nullptr, &vkContext.device);
    assert(res == VK_SUCCESS);
}

/*
=========
setupDebugCallbacks
=========
*/
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
    assert(res == VK_SUCCESS);

    return debugCallback;
}

/*
==================

Swapchain

==================
*/

/*
=========
createSurface
=========
*/
void RenderBackend::createSurface(GLFWwindow* window) {

    auto res = glfwCreateWindowSurface(vkContext.vulkanInstance, window, nullptr, &m_swapchain.surface);
    assert(res == VK_SUCCESS);
}

/*
=========
chooseSurfaceFormat
=========
*/
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

/*
=========
createSwapChain
=========
*/
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
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkContext.physicalDevice, m_swapchain.surface, &surfaceCapabilities);

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

    auto res = vkCreateSwapchainKHR(vkContext.device, &swapchainInfo, nullptr, &m_swapchain.vulkanHandle);
    assert(res == VK_SUCCESS);
}

/*
=========
getSwapchainImages
=========
*/
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

/*
=========
presentImage
=========
*/
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

    vkQueuePresentKHR(vkContext.presentQueue, &present);

    assert(presentResult == VK_SUCCESS);
}

/*
=========
setupImgui
=========
*/
void RenderBackend::setupImgui(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    const auto colorBuffer = Attachment(
        m_swapchain.imageHandles[0],
        0,
        0,
        AttachmentLoadOp::Load);

    m_ui.renderPass = createVulkanRenderPass(std::vector<Attachment> {colorBuffer});
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
    init_info.ImageCount = m_swapchain.imageHandles.size();
    init_info.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&init_info, m_ui.renderPass);

    /*
    build fonts texture
    */
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

    /*
    create framebuffers
    */
    VkExtent2D extent;
    extent.width  = m_images[m_swapchain.imageHandles[0].index].extent.width;
    extent.height = m_images[m_swapchain.imageHandles[0].index].extent.height;
    for (const auto& imageHandle : m_swapchain.imageHandles) {
        const auto attachment = Attachment(
            imageHandle,
            0,
            0,
            AttachmentLoadOp::Load);
        VkFramebuffer framebuffer = createFramebuffer(m_ui.renderPass, extent, std::vector<Attachment> { attachment });
        m_ui.framebuffers.push_back(framebuffer);
    }

    /*
    pass infos
    */
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

/*
==================

resources

==================
*/

/*
=========
createImageView
=========
*/
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
    assert(res == VK_SUCCESS);

    return view;
}

/*
=========
createMeshInternal
=========
*/
MeshHandle RenderBackend::createMeshInternal(const MeshDataInternal data, const std::vector<RenderPassHandle>& passes) {
    std::vector<uint32_t> queueFamilies = { vkContext.queueFamilies.graphicsQueueIndex };
    Mesh mesh;
    mesh.indexCount = data.indices.size();

    /*
    index buffer
    */

    if (mesh.indexCount < std::numeric_limits<uint16_t>::max()) {
        //half precision indices are enough
        mesh.indexPrecision = VK_INDEX_TYPE_UINT16;
        VkDeviceSize indexDataSize = data.indices.size() * sizeof(uint16_t);
        mesh.indexBuffer = createBufferInternal(indexDataSize, queueFamilies, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        //calculate lower precision indices
        std::vector<uint16_t> halfPrecisionIndices;
        halfPrecisionIndices.reserve(data.indices.size());
        for (const auto index : data.indices) {
            halfPrecisionIndices.push_back((uint16_t)index);
        }
        fillBuffer(mesh.indexBuffer, halfPrecisionIndices.data(), indexDataSize);
    }
    else {
        //full precision required
        mesh.indexPrecision = VK_INDEX_TYPE_UINT32;
        VkDeviceSize indexDataSize = data.indices.size() * sizeof(uint32_t);
        mesh.indexBuffer = createBufferInternal(indexDataSize, queueFamilies, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        fillBuffer(mesh.indexBuffer, data.indices.data(), indexDataSize);
    }
    

    /*
    vertex buffer per pass
    */
    for (const auto passHandle : passes) {

        assert(m_renderPasses.isGraphicPassHandle(passHandle));
        const auto& pass = m_renderPasses.getGraphicPassRefByHandle(passHandle);

        /*
        check if there exists a buffer with the required vertex input
        only add new vertex buffer if thats not the case
        */
        bool foundSameInput = false;
        for (auto& buffer : mesh.vertexBuffers) {
            if (pass.vertexInputFlags == buffer.flags) {
                foundSameInput = true;
                break;
            }
        }
        if (foundSameInput) {
            continue;
        }

        /*
        create new buffer
        */
        //using fixed size type to guarantee byte size
        std::vector<uint8_t> vertexData;

        size_t nVertices = 0;
        if (pass.vertexInputFlags & VERTEX_INPUT_POSITION_BIT) {
            nVertices = data.positions.size();
        }
        else if (pass.vertexInputFlags & VERTEX_INPUT_UV_BIT) {
            nVertices = data.uvs.size();
        }
        else if (pass.vertexInputFlags & VERTEX_INPUT_NORMAL_BIT) {
            nVertices = data.normals.size();
        }
        else if (pass.vertexInputFlags & VERTEX_INPUT_TANGENT_BIT) {
            nVertices = data.tangents.size();
        }
        else if (pass.vertexInputFlags & VERTEX_INPUT_BITANGENT_BIT) {
            nVertices = data.bitangents.size();
        }

        //fill in vertex data
        //precision and type must correspond to types in VertexInput.h
        for (size_t i = 0; i < nVertices; i++) {
            if (pass.vertexInputFlags & VERTEX_INPUT_POSITION_BIT) {

                vertexData.push_back(((uint8_t*)&data.positions[i].x)[0]);
                vertexData.push_back(((uint8_t*)&data.positions[i].x)[1]);
                vertexData.push_back(((uint8_t*)&data.positions[i].x)[2]);
                vertexData.push_back(((uint8_t*)&data.positions[i].x)[3]);

                vertexData.push_back(((uint8_t*)&data.positions[i].y)[0]);
                vertexData.push_back(((uint8_t*)&data.positions[i].y)[1]);
                vertexData.push_back(((uint8_t*)&data.positions[i].y)[2]);
                vertexData.push_back(((uint8_t*)&data.positions[i].y)[3]);

                vertexData.push_back(((uint8_t*)&data.positions[i].z)[0]);
                vertexData.push_back(((uint8_t*)&data.positions[i].z)[1]);
                vertexData.push_back(((uint8_t*)&data.positions[i].z)[2]);
                vertexData.push_back(((uint8_t*)&data.positions[i].z)[3]);
            }
            if (pass.vertexInputFlags & VERTEX_INPUT_UV_BIT) {
                //stored as 16 bit signed float
                const auto uHalf = glm::packHalf(glm::vec1(data.uvs[i].x));
                vertexData.push_back(((uint8_t*)&uHalf)[0]);
                vertexData.push_back(((uint8_t*)&uHalf)[1]);

                const auto vHalf = glm::packHalf(glm::vec1(data.uvs[i].y));
                vertexData.push_back(((uint8_t*)&vHalf)[0]);
                vertexData.push_back(((uint8_t*)&vHalf)[1]);
            }
            if (pass.vertexInputFlags & VERTEX_INPUT_NORMAL_BIT) {
                //stored as 32 bit R10G10B10A2
                const auto converted = vec3ToNormalizedR10B10G10A2(data.normals[i]);

                vertexData.push_back(((uint8_t*)&converted)[0]);
                vertexData.push_back(((uint8_t*)&converted)[1]);
                vertexData.push_back(((uint8_t*)&converted)[2]);
                vertexData.push_back(((uint8_t*)&converted)[3]);
            }
            if (pass.vertexInputFlags & VERTEX_INPUT_TANGENT_BIT) {
                //stored as 32 bit R10G10B10A2
                const auto converted = vec3ToNormalizedR10B10G10A2(data.tangents[i]);

                vertexData.push_back(((uint8_t*)&converted)[0]);
                vertexData.push_back(((uint8_t*)&converted)[1]);
                vertexData.push_back(((uint8_t*)&converted)[2]);
                vertexData.push_back(((uint8_t*)&converted)[3]);
            }
            if (pass.vertexInputFlags & VERTEX_INPUT_BITANGENT_BIT) {
                //stored as 32 bit R10G10B10A2
                const auto converted = vec3ToNormalizedR10B10G10A2(data.bitangents[i]);

                vertexData.push_back(((uint8_t*)&converted)[0]);
                vertexData.push_back(((uint8_t*)&converted)[1]);
                vertexData.push_back(((uint8_t*)&converted)[2]);
                vertexData.push_back(((uint8_t*)&converted)[3]);
            }
        }

        /*
        create vertex buffer
        */
        MeshVertexBuffer buffer;
        buffer.flags = pass.vertexInputFlags;
        VkDeviceSize vertexDataSize = vertexData.size() * sizeof(uint8_t);

        buffer.buffer = createBufferInternal(vertexDataSize, queueFamilies,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        fillBuffer(buffer.buffer, vertexData.data(), vertexDataSize);

        mesh.vertexBuffers.push_back(buffer);
    }

    /*
    material per pass
    */
    ImageHandle albedoTexture = data.diffuseTexture;
    ImageHandle normalTexture = data.normalTexture;
    ImageHandle specularTexture = data.specularTexture;

    std::optional<SamplerHandle> albedoSampler;
    std::optional<SamplerHandle> normalSampler;
    std::optional<SamplerHandle> specularSampler;

    for (const auto passHandle : passes) {

        const auto& pass = m_renderPasses.getGraphicPassRefByHandle(passHandle);

        /*
        check if there exists a material with the required features
        only add new material if thats not the case
        */
        bool foundSameMaterial = false;
        for (auto& material : mesh.materials) {
            if (pass.materialFeatures == material.flags) {
                foundSameMaterial = true;
                break;
            }
        }
        if (foundSameMaterial) {
            continue;
        }

        /*
        create resources
        */
        RenderPassResources resources;

        //add resources depending on material flags
        if (pass.materialFeatures & MATERIAL_FEATURE_FLAG_ALBEDO_TEXTURE) {

            //create sampler if needed
            if (!albedoSampler.has_value()) {
                if (albedoTexture.index != invalidIndex) {
                    albedoSampler = m_materialSamplers.albedoSampler;
                }
                else {
                    std::cout << "Mesh misses required albedo texture \n";
                }
            }

            const auto albedoTextureResource = ImageResource(
                albedoTexture,
                0,
                3);

            const auto albedoSamplerResource = SamplerResource(
                albedoSampler.value(),
                0);

            resources.sampledImages.push_back(albedoTextureResource);
            resources.samplers.push_back(albedoSamplerResource);
        }
        if (pass.materialFeatures & MATERIAL_FEATURE_FLAG_NORMAL_TEXTURE) {
            if (!normalSampler.has_value()) {
                if (normalTexture.index != invalidIndex) {
                    normalSampler = m_materialSamplers.normalSampler;
                }
                else {
                    std::cout << "Mesh misses required normal texture \n";
                }
            }

            const auto normalTextureResource = ImageResource(
                normalTexture,
                0,
                4);

            const auto normalSamplerResource = SamplerResource(
                normalSampler.value(),
                1);

            resources.sampledImages.push_back(normalTextureResource);
            resources.samplers.push_back(normalSamplerResource);

        }
        if (pass.materialFeatures & MATERIAL_FEATURE_FLAG_SPECULAR_TEXTURE) {
            if (!specularSampler.has_value()) {
                if (specularTexture.index != invalidIndex) {
                    specularSampler = m_materialSamplers.specularSampler;
                }
                else {
                    std::cout << "Mesh misses required specular texture \n";
                }
            }

            const auto specularTextureResource = ImageResource(
                specularTexture,
                0,
                5);

            const auto specularSamplerResource = SamplerResource(
                specularSampler.value(),
                2);

            resources.sampledImages.push_back(specularTextureResource);
            resources.samplers.push_back(specularSamplerResource);
        }
        MeshMaterial material;
        material.flags = pass.materialFeatures;
        const auto setSizes = descriptorSetAllocationSizeFromMaterialFlags(pass.materialFeatures);
        material.descriptorSet = allocateDescriptorSet(pass.materialSetLayout, setSizes);
        updateDescriptorSet(material.descriptorSet, resources);

        mesh.materials.push_back(material);
    }

    /*
    save and return handle
    */
    MeshHandle handle = { m_meshes.size() };
    m_meshes.push_back(mesh);

    return handle;
}

/*
=========
createDynamicMeshInternal
=========
*/
DynamicMeshHandle RenderBackend::createDynamicMeshInternal(const uint32_t maxPositions, const uint32_t maxIndices) {
    
    DynamicMesh mesh;
    mesh.indexCount = 0;

    std::vector<uint32_t> queueFamilies = { vkContext.queueFamilies.graphicsQueueIndex };
    const uint32_t memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    //create vertex buffer    
    mesh.vertexBuffer.flags = VERTEX_INPUT_POSITION_BIT;
    const uint32_t floatsPerPosition = 3; //xyz
    VkDeviceSize vertexDataSize = maxPositions * sizeof(float) * floatsPerPosition;

    mesh.vertexBuffer.buffer = createBufferInternal(vertexDataSize, queueFamilies,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, memoryFlags);

    //create index buffer
    VkDeviceSize indexDataSize = maxIndices * sizeof(uint32_t);
    mesh.indexBuffer = createBufferInternal(indexDataSize, queueFamilies, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, memoryFlags);

    //save and return handle
    DynamicMeshHandle handle = { m_dynamicMeshes.size() };
    m_dynamicMeshes.push_back(mesh);

    return handle;
}

/*
=========
createBufferInternal
=========
*/
Buffer RenderBackend::createBufferInternal(const VkDeviceSize size, const std::vector<uint32_t>& queueFamilies, const VkBufferUsageFlags usage, const uint32_t memoryFlags) {

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.flags = 0;
    bufferInfo.size = size;
    bufferInfo.usage = usage;

    /*
    find unique queue families
    */
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

    bufferInfo.queueFamilyIndexCount = uniqueQueueFamilies.size();
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

/*
=========
createSubresourceLayers
=========
*/
VkImageSubresourceLayers RenderBackend::createSubresourceLayers(const Image& image, const uint32_t mipLevel) {
    VkImageSubresourceLayers layers;
    layers.aspectMask = isDepthFormat(image.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    layers.mipLevel = mipLevel;
    layers.baseArrayLayer = 0;
    layers.layerCount = 1;
    return layers;
}

/*
=========
transferDataIntoImage
=========
*/
void RenderBackend::transferDataIntoImage(Image& target, const void* data, const VkDeviceSize size) {

    //BCn compressed formats have certain properties that have to be considered when copying data
    bool isBCnCompressed = false;

    //bytePerPixel is a float because compressed formats can have less than one byte per pixel
    float bytePerPixel = 0;
    if (target.desc.format == ImageFormat::R8) {
        bytePerPixel = 1;
    }
    else if (target.desc.format == ImageFormat::R11G11B10_uFloat) {
        bytePerPixel = 4;
    }
    else if (target.desc.format == ImageFormat::RG16_sFloat) {
        bytePerPixel = 4;
    }
    else if (target.desc.format == ImageFormat::RG32_sFloat) {
        bytePerPixel = 8;
    }
    else if (target.desc.format == ImageFormat::RG8) {
        bytePerPixel = 2;
    }
    else if (target.desc.format == ImageFormat::RGBA16_sFloat) {
        bytePerPixel = 8;
    }
    else if (target.desc.format == ImageFormat::RGBA32_sFloat) {
        bytePerPixel = 16;
    }
    else if (target.desc.format == ImageFormat::RGBA8) {
        bytePerPixel = 4;
    }
    else if (target.desc.format == ImageFormat::BC1) {
        isBCnCompressed = true;
        bytePerPixel = 0.5;
    }
    else if (target.desc.format == ImageFormat::BC3) {
        isBCnCompressed = true;
        bytePerPixel = 1;
    }
    else if (target.desc.format == ImageFormat::BC5) {
        isBCnCompressed = true;
        bytePerPixel = 1;
    }
    else {
        throw("Unsupported format");
    }

    VkDeviceSize bytesPerRow = target.extent.width * bytePerPixel;

    //if size is bigger than mip level 0 automatically switch to next mip level
    uint32_t mipLevel = 0;
    uint32_t currentMipWidth = target.extent.width;
    uint32_t currentMipHeight = target.extent.height;
    VkDeviceSize currentMipSize = currentMipWidth * currentMipHeight * bytePerPixel;

    //memory offset per mip is tracked separately to check if a mip border is reached
    uint32_t mipMemoryOffset = 0;

    //total offset is used to check if entire data has been copied
    VkDeviceSize totalMemoryOffset = 0;

    /*
    if the image data is bigger than the staging buffer multiple copies are needed
    use a while loop because currentMemoryOffset is increased by copySize, which can vary at mip borders
    */
    //TODO: creation of cmd buffer and fence in loop is somewhat inefficient
    while (totalMemoryOffset < size) {

        //check if mip border is reached
        if (mipMemoryOffset >= currentMipSize) {
            mipLevel++;
            //resoltion is halved at every mip level
            currentMipWidth /= 2;
            currentMipHeight /= 2;
            bytesPerRow /= 2;

            //halving resolution means size is quartered
            currentMipSize /= 4;

            //memory offset per mip is reset
            mipMemoryOffset = 0;

            //BCn compressed textures store at least a 4x4 pixel block, resulting in at least a 4 pixel row
            if (isBCnCompressed) {
                bytesPerRow = std::max(bytesPerRow, (VkDeviceSize)(4 * bytePerPixel));
                currentMipSize = std::max(currentMipSize, (VkDeviceSize)(4 * 4 * bytePerPixel));
            }
        }

        /*
        the size to copy is limited either by
        -the staging buffer size
        -the size left to copy on the current mip level
        */
        VkDeviceSize copySize = std::min(m_stagingBufferSize, currentMipSize - mipMemoryOffset);

        //always copy entire rows
        copySize = copySize / bytesPerRow * bytesPerRow;

        //copy data to staging buffer
        void* mappedData;
        fillHostVisibleCoherentBuffer(m_stagingBuffer, (char*)data + totalMemoryOffset, copySize);

        //begin command buffer for copying
        VkCommandBuffer copyBuffer = beginOneTimeUseCommandBuffer();

        //layout transition to transfer_dst the first time
        if (totalMemoryOffset == 0) {
            const auto toTransferDstBarrier = createImageBarriers(target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_ACCESS_TRANSFER_READ_BIT, 0, target.viewPerMip.size());

            barriersCommand(copyBuffer, toTransferDstBarrier, std::vector<VkBufferMemoryBarrier> {});
        }
        

        //calculate which region to copy
        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.imageSubresource = createSubresourceLayers(target, mipLevel);
        //entire rows are copied, so the starting row(offset.y) is the current mip offset divided by the row size
        region.imageOffset = { 0, (int32_t)(mipMemoryOffset / bytesPerRow), 0 };
        region.bufferRowLength = currentMipWidth; 
        region.bufferImageHeight = currentMipHeight;
        //copy as many rows as fit into the copy size, without going over the mip height
        region.imageExtent.height = std::min(copySize / bytesPerRow, (VkDeviceSize)currentMipHeight);
        region.imageExtent.width = currentMipWidth;
        region.imageExtent.depth = 1;

        //BCn compressed textures are stored in 4x4 pixel blocks, so that is the minimum buffer size
        if (isBCnCompressed) {
            region.bufferRowLength      = std::max(region.bufferRowLength,      (uint32_t)4);;
            region.bufferImageHeight    = std::max(region.bufferImageHeight,    (uint32_t)4);;
        }

        //issue for commands, then wait
        vkCmdCopyBufferToImage(copyBuffer, m_stagingBuffer.vulkanHandle, target.vulkanHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        vkEndCommandBuffer(copyBuffer);
        VkFence fence = submitOneTimeUseCmdBuffer(copyBuffer, vkContext.transferQueue);
        vkWaitForFences(vkContext.device, 1, &fence, VK_TRUE, UINT64_MAX);

        //cleanup
        vkDestroyFence(vkContext.device, fence, nullptr);
        vkFreeCommandBuffers(vkContext.device, m_transientCommandPool, 1, &copyBuffer);

        //update memory offsets
        //BCn compressed textures store at least a 4x4 pixel block
        if (isBCnCompressed) {
            mipMemoryOffset     += std::max(copySize, (VkDeviceSize)(4 * 4 * bytePerPixel));
            totalMemoryOffset   += std::max(copySize, (VkDeviceSize)(4 * 4 * bytePerPixel));
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
    const auto newLayoutBarriers = createImageBarriers(image, newLayout, VK_ACCESS_TRANSFER_WRITE_BIT, 0, image.viewPerMip.size());
    barriersCommand(blitCmdBuffer, newLayoutBarriers, std::vector<VkBufferMemoryBarrier> {});

    //end recording
    vkEndCommandBuffer(blitCmdBuffer);

    //submit
    VkFence fence = submitOneTimeUseCmdBuffer(blitCmdBuffer, vkContext.transferQueue);

    vkWaitForFences(vkContext.device, 1, &fence, VK_TRUE, UINT64_MAX);

    //cleanup
    vkDestroyFence(vkContext.device, fence, nullptr);
    vkFreeCommandBuffers(vkContext.device, m_transientCommandPool, 1, &blitCmdBuffer);
}

/*
=========
fillBuffer
=========
*/
void RenderBackend::fillBuffer(Buffer target, const void* data, const VkDeviceSize size) {

    //TODO: creation of cmd buffer and fence in loop is somewhat inefficient
    for (VkDeviceSize currentMemoryOffset = 0; currentMemoryOffset < size; currentMemoryOffset += m_stagingBufferSize) {

        VkDeviceSize copySize = std::min(m_stagingBufferSize, size - currentMemoryOffset);

        //copy data to staging buffer
        void* mappedData;
        vkMapMemory(vkContext.device, m_stagingBuffer.memory.vkMemory, 0, copySize, 0, (void**)&mappedData);
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
        vkEndCommandBuffer(copyCmdBuffer);       

        //submit and wait
        VkFence fence = submitOneTimeUseCmdBuffer(copyCmdBuffer, vkContext.transferQueue);
        vkWaitForFences(vkContext.device, 1, &fence, VK_TRUE, UINT64_MAX);

        //cleanup
        vkDestroyFence(vkContext.device, fence, nullptr);
        vkFreeCommandBuffers(vkContext.device, m_transientCommandPool, 1, &copyCmdBuffer);
    }
}

/*
=========
fillHostVisibleCoherentBuffer
=========
*/
void RenderBackend::fillHostVisibleCoherentBuffer(Buffer target, const void* data, const VkDeviceSize size) {
    void* mappedData;
    vkMapMemory(vkContext.device, target.memory.vkMemory, target.memory.offset, size, 0, (void**)&mappedData);
    memcpy(mappedData, data, size);
    vkUnmapMemory(vkContext.device, m_stagingBuffer.memory.vkMemory);
}

/*
==================

commands

==================
*/

/*
=========
createCommandPool
=========
*/
VkCommandPool RenderBackend::createCommandPool(const uint32_t queueFamilyIndex, const VkCommandPoolCreateFlagBits flags) {

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = flags;
    poolInfo.queueFamilyIndex = queueFamilyIndex;

    VkCommandPool pool;
    auto res = vkCreateCommandPool(vkContext.device, &poolInfo, nullptr, &pool);
    assert(res == VK_SUCCESS);

    return pool;
}

/*
=========
allocateCommandBuffer
=========
*/
VkCommandBuffer RenderBackend::allocateCommandBuffer() {

    VkCommandBufferAllocateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.commandPool = m_commandPool;
    bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    bufferInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    auto res = vkAllocateCommandBuffers(vkContext.device, &bufferInfo, &commandBuffer);
    assert(res == VK_SUCCESS);

    return commandBuffer;
}

/*
=========
beginOneTimeUseCommandBuffer
=========
*/
VkCommandBuffer RenderBackend::beginOneTimeUseCommandBuffer() {

    //allocate copy command buffer
    VkCommandBufferAllocateInfo command = {};
    command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command.pNext = nullptr;
    command.commandPool = m_transientCommandPool;
    command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(vkContext.device, &command, &cmdBuffer);

    //begin recording
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = VK_NULL_HANDLE;

    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    return cmdBuffer;
}

/*
=========
SubmitOneTimeUseCmdBuffer
=========
*/
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
    vkResetFences(vkContext.device, 1, &fence);

    vkQueueSubmit(queue, 1, &submit, fence);

    return fence;
}

/*
=========
SubmitOneTimeUseCmdBuffer
=========
*/
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

/*
=========
endDebugLabel
=========
*/
void RenderBackend::endDebugLabel(const VkCommandBuffer cmdBuffer) {
    m_debugExtFunctions.vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
}


/*
==================

descriptors and layouts

==================
*/

/*
=========
createImguiDescriptorPool
=========
*/
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
    assert(res == VK_SUCCESS);
}

/*
=========
createDescriptorPool
=========
*/
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
    assert(res == VK_SUCCESS);
    
    m_descriptorPools.push_back(pool);
}

/*
=========
descriptorSetAllocationSizeFromShaderReflection
=========
*/
DescriptorPoolAllocationSizes RenderBackend::descriptorSetAllocationSizeFromShaderReflection(const ShaderReflection& reflection) {
    DescriptorPoolAllocationSizes sizes;
    sizes.setCount = 1;
    const auto& shaderLayout = reflection.shaderLayout;
    sizes.imageSampled = shaderLayout.sampledImageBindings.size();
    sizes.imageStorage = shaderLayout.storageImageBindings.size();
    sizes.storageBuffer = shaderLayout.storageBufferBindings.size();
    sizes.uniformBuffer = shaderLayout.uniformBufferBindings.size();
    sizes.sampler = shaderLayout.samplerBindings.size();
    return sizes;
}

/*
=========
descriptorSetAllocationSizeFromMaterialFlags
=========
*/
DescriptorPoolAllocationSizes RenderBackend::descriptorSetAllocationSizeFromMaterialFlags(const MaterialFeatureFlags& flags) {
    
    DescriptorPoolAllocationSizes sizes;
    sizes.setCount = 1;

    const MaterialFeatureFlags materialFlagsBits[] = {
        MaterialFeatureFlags::MATERIAL_FEATURE_FLAG_ALBEDO_TEXTURE,
        MaterialFeatureFlags::MATERIAL_FEATURE_FLAG_NORMAL_TEXTURE,
        MaterialFeatureFlags::MATERIAL_FEATURE_FLAG_SPECULAR_TEXTURE
    };
    for (const MaterialFeatureFlags feature : materialFlagsBits) {
        if (flags & feature) {
            //every material flag corresponds to a sampled texture and it's sampler
            sizes.imageSampled += 1;
            sizes.sampler += 1;
        }
    }
    return sizes;
}

/*
=========
allocateDescriptorSet
=========
*/
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
    vkAllocateDescriptorSets(vkContext.device, &setInfo, &descriptorSet);

    return descriptorSet;
}

/*
=========
updateDescriptorSet
=========
*/
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

    vkUpdateDescriptorSets(vkContext.device, descriptorInfos.size(), descriptorInfos.data(), 0, nullptr);
}

/*
=========
createDescriptorSetLayout
=========
*/
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
    layoutInfo.bindingCount = layoutBindings.size();
    layoutInfo.pBindings = layoutBindings.data();

    VkDescriptorSetLayout setLayout;
    auto res = vkCreateDescriptorSetLayout(vkContext.device, &layoutInfo, nullptr, &setLayout);
    assert(res == VK_SUCCESS);
    return setLayout;
}

/*
=========
createPipelineLayout
=========
*/
VkPipelineLayout RenderBackend::createPipelineLayout(const VkDescriptorSetLayout setLayout, const VkDescriptorSetLayout materialSetLayout, 
    const bool isGraphicPass) {

    VkPushConstantRange matrices = {};
    matrices.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    matrices.offset = 0;
    matrices.size = 128;

    VkDescriptorSetLayout setLayouts[3] = { m_globalDescriptorSetLayout, setLayout, materialSetLayout };
    uint32_t setCount = materialSetLayout != VK_NULL_HANDLE ? 3 : 2;

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
    assert(res == VK_SUCCESS);

    return layout;
}

/*
==================

renderpass creation

==================
*/

/*
=========
createComputePassInternal
=========
*/
ComputePass RenderBackend::createComputePassInternal(const ComputePassDescription& desc, const std::vector<uint32_t>& spirV) {

    ComputePass pass;
    pass.computePassDesc = desc;
    VkComputePipelineCreateInfo pipelineInfo;

    VkShaderModule module = createShaderModule(spirV);
    ShaderReflection reflection = performComputeShaderReflection(spirV);
    pass.descriptorSetLayout = createDescriptorSetLayout(reflection.shaderLayout);
    pass.pipelineLayout = createPipelineLayout(pass.descriptorSetLayout, VK_NULL_HANDLE, false);

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
    assert(res == VK_SUCCESS);

    //shader module no needed anymore
    vkDestroyShaderModule(vkContext.device, module, nullptr);

    /*
    descriptor set
    */
    const auto setSizes = descriptorSetAllocationSizeFromShaderReflection(reflection);
    pass.descriptorSet = allocateDescriptorSet(pass.descriptorSetLayout, setSizes);

    return pass;
}

/*
=========
createGraphicPassInternal
=========
*/
GraphicPass RenderBackend::createGraphicPassInternal(const GraphicPassDescription& desc, const GraphicPassShaderSpirV& spirV) {

    GraphicPass pass;
    pass.graphicPassDesc = desc;
    pass.attachmentDescriptions = desc.attachments;
    for (const auto attachment : desc.attachments) {
        pass.attachments.push_back(attachment.image);
    }

    /*
    load shader modules
     */
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
        tesselationControlModule    = createShaderModule(spirV.tesselationControl.value());
        tesselationEvaluationModule = createShaderModule(spirV.tesselationEvaluation.value());
    }

    /*
    create module infos
    */
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

    /*
    shader reflection
    */
    ShaderReflection reflection = performShaderReflection(spirV);
    pass.vertexInputFlags = reflection.vertexInputFlags;
    pass.materialFeatures = reflection.materialFeatures;
    pass.descriptorSetLayout = createDescriptorSetLayout(reflection.shaderLayout);

    /*
    material set layout
    */
    ShaderLayout shaderLayout;
    if (reflection.materialFeatures & MATERIAL_FEATURE_FLAG_ALBEDO_TEXTURE) {
        shaderLayout.samplerBindings.push_back(0);
        shaderLayout.sampledImageBindings.push_back(3);
    }
    if (reflection.materialFeatures & MATERIAL_FEATURE_FLAG_NORMAL_TEXTURE) {
        shaderLayout.samplerBindings.push_back(1);
        shaderLayout.sampledImageBindings.push_back(4);
    }
    if (reflection.materialFeatures & MATERIAL_FEATURE_FLAG_SPECULAR_TEXTURE) {
        shaderLayout.samplerBindings.push_back(2);
        shaderLayout.sampledImageBindings.push_back(5);
    }
    pass.materialSetLayout = createDescriptorSetLayout(shaderLayout);
    pass.pipelineLayout = createPipelineLayout(pass.descriptorSetLayout, pass.materialSetLayout, true);

    /*
    get width and height from output
    */
    assert(desc.attachments.size() >= 1); //need at least a single attachment to write to
    const uint32_t width = m_images[desc.attachments[0].image.index].extent.width;
    const uint32_t height = m_images[desc.attachments[0].image.index].extent.height;

    /*
    validate attachments
    */
    for (const auto attachmentDefinition : desc.attachments) {

        const Image attachment = m_images[attachmentDefinition.image.index];

        //all attachments need same resolution
        assert(attachment.extent.width == width);
        assert(attachment.extent.height == height);

        //attachment must be 2D
        assert(attachment.extent.depth == 1);
    }

    /*
    vertex input
    */
    std::vector<VkVertexInputAttributeDescription> attributes;
    uint32_t currentOffset = 0;

    for (size_t location = 0; location < VERTEX_INPUT_ATTRIBUTE_COUNT; location++) {
        if (vertexInputFlagPerLocation[location] & reflection.vertexInputFlags) {
            VkVertexInputAttributeDescription attribute;
            attribute.location = location;
            attribute.binding = 0;
            attribute.format = vertexInputFormatsPerLocation[location];
            attribute.offset = currentOffset;
            attributes.push_back(attribute);
            currentOffset += vertexInputBytePerLocation[location];
        }
    }

    VkVertexInputBindingDescription vertexBinding;
    vertexBinding.binding = 0;
    vertexBinding.stride = currentOffset;
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.pNext = nullptr;
    vertexInputInfo.flags = 0;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexBinding;
    vertexInputInfo.vertexAttributeDescriptionCount = attributes.size();
    vertexInputInfo.pVertexAttributeDescriptions = attributes.data();

    /*
    renderpass
    */
    pass.vulkanRenderPass = createVulkanRenderPass(desc.attachments);

    /*
    viewport settings
    */
    VkExtent2D extent;
    extent.width = width;
    extent.height = height;

    pass.scissor.offset = { 0, 0 };
    pass.scissor.extent = extent;

    pass.viewport.x = 0;
    pass.viewport.y = 0;
    pass.viewport.width = width;
    pass.viewport.height = height;
    pass.viewport.minDepth = 0.f;
    pass.viewport.maxDepth = 1.f;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;
    viewportState.flags = 0;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    /*
    blending
    */

    /*
    only global blending state for all attachments
    currently only no blending and additive supported
    */
    VkPipelineColorBlendAttachmentState blendingAttachment = {};
    blendingAttachment.blendEnable = desc.blending != BlendState::None ? VK_TRUE : VK_FALSE;
    blendingAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendingAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendingAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendingAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendingAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendingAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendingAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    std::vector<VkPipelineColorBlendAttachmentState> blendingAttachments;
    for (const auto& attachment : desc.attachments) {
        const auto image = m_images[attachment.image.index];
        if (!isDepthFormat(image.format)) {
            blendingAttachments.push_back(blendingAttachment);
        }
    }

    //color blending
    VkPipelineColorBlendStateCreateInfo blending = {};
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.pNext = nullptr;
    blending.flags = 0;
    blending.logicOpEnable = VK_FALSE;
    blending.logicOp = VK_LOGIC_OP_NO_OP;
    blending.attachmentCount = blendingAttachments.size();
    blending.pAttachments = blendingAttachments.data();
    blending.blendConstants[0] = 0.f;
    blending.blendConstants[1] = 0.f;
    blending.blendConstants[2] = 0.f;
    blending.blendConstants[3] = 0.f;

    /*
    graphic pipeline
    */
    const auto rasterizationState = createRasterizationState(desc.rasterization);
    const auto multisamplingState = createDefaultMultisamplingInfo();
    const auto depthStencilState = createDepthStencilState(desc.depthTest);
    const auto tesselationState = desc.shaderDescriptions.tesselationControl.has_value() ? &createTesselationState(desc.patchControlPoints) : nullptr;
    auto inputAssemblyState = createDefaultInputAssemblyInfo();

    if (desc.rasterization.mode == RasterizationeMode::Line) {
        inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    }
    else if (desc.rasterization.mode == RasterizationeMode::Point) {
        inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    }

    /*
    dynamic state
    */
    std::vector<VkDynamicState> dynamicStates;
    dynamicStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

    VkPipelineDynamicStateCreateInfo dynamicStateInfo;
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.pNext = nullptr;
    dynamicStateInfo.flags = 0;
    dynamicStateInfo.dynamicStateCount = dynamicStates.size();
    dynamicStateInfo.pDynamicStates = dynamicStates.data();


    VkGraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.flags = 0;
    pipelineInfo.stageCount = stages.size();
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineInfo.pTessellationState = tesselationState;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizationState;
    pipelineInfo.pMultisampleState = &multisamplingState;
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.pColorBlendState = &blending;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.layout = pass.pipelineLayout;
    pipelineInfo.renderPass = pass.vulkanRenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = 0;

    vkCreateGraphicsPipelines(vkContext.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pass.pipeline);

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

    /*
    clear values
    */
    for (const auto& attachment : desc.attachments) {
        const auto image = m_images[attachment.image.index];
        if (!isDepthFormat(image.format)) {
            VkClearValue colorClear = {};
            colorClear.color = { 0, 0, 0, 0 };
            pass.clearValues.push_back(colorClear);
        }
        else {
            VkClearValue depthClear = {};
            depthClear.depthStencil.depth = 1.f;
            pass.clearValues.push_back(depthClear);
        }
    }

    /*
    create pass begin info
    */
    VkRect2D rect = {};
    rect.extent = extent;
    rect.offset = { 0, 0 };

    pass.beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass.beginInfo.pNext = nullptr;
    pass.beginInfo.renderPass = pass.vulkanRenderPass;
    pass.beginInfo.framebuffer = createFramebuffer(pass.vulkanRenderPass, extent, desc.attachments);
    pass.beginInfo.clearValueCount = pass.clearValues.size();
    pass.beginInfo.pClearValues = pass.clearValues.data();
    pass.beginInfo.renderArea = rect;

    /*
    descriptor set
    */
    const auto setSizes = descriptorSetAllocationSizeFromShaderReflection(reflection);
    pass.descriptorSet = allocateDescriptorSet(pass.descriptorSetLayout, setSizes);

    return pass;
}

/*
=========
createVulkanRenderPass
=========
*/
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

        const auto image = m_images[attachment.image.index];
        VkImageLayout layout = isDepthFormat(image.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        desc.flags = 0;
        desc.format = m_images[attachment.image.index].format;
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
    subpass.colorAttachmentCount = colorReferences.size();
    subpass.pColorAttachments = colorReferences.data();
    subpass.pResolveAttachments = nullptr;
    subpass.pDepthStencilAttachment = hasDepth ? &depthReference : nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = nullptr;

    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.attachmentCount = descriptions.size();
    info.pAttachments = descriptions.data();
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 0;
    info.pDependencies = nullptr;

    auto res = vkCreateRenderPass(vkContext.device, &info, nullptr, &pass);
    assert(res == VK_SUCCESS);
    return pass;
}

/*
=========
createFramebuffer
=========
*/
VkFramebuffer RenderBackend::createFramebuffer(const VkRenderPass renderPass, const VkExtent2D extent, const std::vector<Attachment>& attachments) {

    std::vector<VkImageView> views;
    for (const auto& attachment : attachments) {
        const auto image = m_images[attachment.image.index];
        const auto view = image.viewPerMip[attachment.mipLevel];
        views.push_back(view);
    }

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.pNext = nullptr;
    framebufferInfo.flags = 0;
    framebufferInfo.renderPass = renderPass;

    framebufferInfo.attachmentCount = views.size();
    framebufferInfo.pAttachments = views.data();

    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1;

    VkFramebuffer framebuffer;
    auto res = vkCreateFramebuffer(vkContext.device, &framebufferInfo, nullptr, &framebuffer);
    assert(res == VK_SUCCESS);

    return framebuffer;
}

/*
=========
createShaderModules
=========
*/
VkShaderModule RenderBackend::createShaderModule(const std::vector<uint32_t>& code) {

    VkShaderModuleCreateInfo moduleInfo;
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.pNext = nullptr;
    moduleInfo.flags = 0;
    moduleInfo.codeSize = code.size() * sizeof(uint32_t);
    moduleInfo.pCode = code.data();

    VkShaderModule shaderModule;
    auto res = vkCreateShaderModule(vkContext.device, &moduleInfo, nullptr, &shaderModule);
    assert(res == VK_SUCCESS);

    return shaderModule;
}

/*
createPipelineShaderStageInfos
*/
VkPipelineShaderStageCreateInfo RenderBackend::createPipelineShaderStageInfos(
    const VkShaderModule module, 
    const VkShaderStageFlagBits stage,
    const std::vector<SpecialisationConstant>& specialisationInfo, 
    VulkanShaderCreateAdditionalStructs* outAdditionalInfo) {

    assert(outAdditionalInfo != nullptr);
    uint32_t specialisationCount = specialisationInfo.size();

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

        uint32_t currentOffset = 0;
        for (uint32_t i = 0; i < outAdditionalInfo->specilisationMap.size(); i++) {
            const auto constant = specialisationInfo[i];

            outAdditionalInfo->specilisationMap[i].constantID = constant.location;
            outAdditionalInfo->specilisationMap[i].offset = currentOffset;
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
        outAdditionalInfo->specialisationInfo.mapEntryCount = specialisationCount;
        outAdditionalInfo->specialisationInfo.pData         = outAdditionalInfo->specialisationData.data();
        outAdditionalInfo->specialisationInfo.pMapEntries   = outAdditionalInfo->specilisationMap.data();

        createInfos.pSpecializationInfo = &outAdditionalInfo->specialisationInfo;
    }

    return createInfos;
}

/*
=========
createDefaultInputAssemblyInfo
=========
*/
VkPipelineInputAssemblyStateCreateInfo RenderBackend::createDefaultInputAssemblyInfo() {
    VkPipelineInputAssemblyStateCreateInfo info;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    info.primitiveRestartEnable = VK_FALSE;
    return info;
}

/*
=========
createTesselationState
=========
*/
VkPipelineTessellationStateCreateInfo RenderBackend::createTesselationState(const uint32_t patchControlPoints) {
    VkPipelineTessellationStateCreateInfo info;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.patchControlPoints = patchControlPoints;
    return info;
}

/*
=========
createRasterizationState
=========
*/
VkPipelineRasterizationStateCreateInfo RenderBackend::createRasterizationState(const RasterizationConfig& raster) {

    VkPolygonMode polygonMode;
    switch (raster.mode) {
    case RasterizationeMode::Fill:  polygonMode = VK_POLYGON_MODE_FILL; break;
    case RasterizationeMode::Line:  polygonMode = VK_POLYGON_MODE_LINE; break;
    case RasterizationeMode::Point:  polygonMode = VK_POLYGON_MODE_POINT; break;
    };

    VkCullModeFlags cullFlags;
    switch (raster.cullMode) {
    case CullMode::None:  cullFlags = VK_CULL_MODE_NONE; break;
    case CullMode::Front:  cullFlags = VK_CULL_MODE_FRONT_BIT; break;
    case CullMode::Back:  cullFlags = VK_CULL_MODE_BACK_BIT; break;
    };

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.pNext = nullptr;
    rasterization.flags = 0;
    rasterization.depthClampEnable = raster.clampDepth;
    rasterization.rasterizerDiscardEnable = VK_FALSE;
    rasterization.polygonMode = polygonMode;
    rasterization.cullMode = cullFlags;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.depthBiasEnable = VK_FALSE;
    rasterization.depthBiasConstantFactor = 0.f;
    rasterization.depthBiasClamp = 0.f;
    rasterization.depthBiasSlopeFactor = 0.f;
    rasterization.lineWidth = 1.f;
    return rasterization;
}

/*
=========
createDefaultMultisamplingInfo
=========
*/
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

/*
=========
createDepthStencilState
=========
*/
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
    VkCompareOp compareOp;
    switch (depthTest.function) {
    case DepthFunction::Always: compareOp = VK_COMPARE_OP_ALWAYS; break;
    case DepthFunction::Equal: compareOp = VK_COMPARE_OP_EQUAL; break;
    case DepthFunction::Greater: compareOp = VK_COMPARE_OP_GREATER; break;
    case DepthFunction::GreaterEqual : compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; break;
    case DepthFunction::Less: compareOp = VK_COMPARE_OP_LESS; break;
    case DepthFunction::LessEqual: compareOp = VK_COMPARE_OP_LESS_OR_EQUAL; break;
    case DepthFunction::Never : compareOp = VK_COMPARE_OP_NEVER; break;
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

/*
=========
isDepthFormat
=========
*/
bool RenderBackend::isDepthFormat(ImageFormat format) {
    return (format == ImageFormat::Depth16 ||
        format == ImageFormat::Depth32);
}

/*
=========
isDepthFormat
=========
*/
bool RenderBackend::isDepthFormat(VkFormat format) {
    return (
        format == VK_FORMAT_D16_UNORM ||
        format == VK_FORMAT_D16_UNORM_S8_UINT ||
        format == VK_FORMAT_D24_UNORM_S8_UINT ||
        format == VK_FORMAT_D32_SFLOAT ||
        format == VK_FORMAT_D32_SFLOAT_S8_UINT);
}

/*
=========
barriersCommand
=========
*/
void RenderBackend::barriersCommand(const VkCommandBuffer commandBuffer, 
    const std::vector<VkImageMemoryBarrier>& imageBarriers, const std::vector<VkBufferMemoryBarrier>& memoryBarriers) {

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, memoryBarriers.size(), 
        memoryBarriers.data(), imageBarriers.size(), imageBarriers.data());
}

/*
=========
createBufferBarriers
=========
*/
void RenderBackend::createBufferBarriers() {
    /*
    FIXME memory barriers not implemented yet for buffers
    */
    throw std::runtime_error("Buffer barriers not yet implemented");
}

/*
=========
createImageBarrier
=========
*/
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
        if (image.layoutPerMip[baseMip + i] == barriers.back().oldLayout) {
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
            barrier.oldLayout = image.layoutPerMip[baseMip + i];
            barrier.newLayout = newLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image.vulkanHandle;
            barrier.subresourceRange.aspectMask = aspectFlags;
            barrier.subresourceRange.baseMipLevel = baseMip + i;
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

/*
=========
createBufferBarrier
=========
*/
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

/*
==================

sync objects

==================
*/

/*
=========
createSemaphore
=========
*/
VkSemaphore RenderBackend::createSemaphore() {

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.flags = 0;

    VkSemaphore semaphore;
    auto res = vkCreateSemaphore(vkContext.device, &semaphoreInfo, nullptr, &semaphore);
    assert(res == VK_SUCCESS);

    return semaphore;
}

/*
=========
createFence
=========
*/
VkFence RenderBackend::createFence() {

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkFence fence;
    auto res = vkCreateFence(vkContext.device, &fenceInfo, nullptr, &fence);
    assert(res == VK_SUCCESS);

    return fence;
}

/*
=========
createQueryPool
=========
*/
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

/*
=========
resetTimestampQueryPool
=========
*/
void RenderBackend::resetTimestampQueryPool() {
    m_timestampQueries.resize(0);
    vkResetQueryPool(vkContext.device, m_timestampQueryPool, 0, m_currentTimestampQueryCount);
    m_currentTimestampQueryCount = 0;
}

/*
=========
issueTimestampQuery
=========
*/
uint32_t RenderBackend::issueTimestampQuery(const VkCommandBuffer cmdBuffer) {
    const uint32_t query = m_currentTimestampQueryCount;
    vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_timestampQueryPool, query);
    m_currentTimestampQueryCount++;
    return query;
}

/*
==================

resource destruction

==================
*/

/*
=========
destroyImage
=========
*/
void RenderBackend::destroyImage(const ImageHandle handle) {

    m_freeImageHandles.push_back(handle);

    const auto image = m_images[handle.index];
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

/*
=========
destroyBuffer
=========
*/
void RenderBackend::destroyBuffer(const Buffer& buffer) {
    vkDestroyBuffer(vkContext.device, buffer.vulkanHandle, nullptr);
    m_vkAllocator.free(buffer.memory);
}

/*
=========
destroyMesh
=========
*/
void RenderBackend::destroyMesh(const Mesh& mesh) {
    for (const auto& buffer : mesh.vertexBuffers) {
        destroyBuffer(buffer.buffer);
    }
    destroyBuffer(mesh.indexBuffer);
}

/*
=========
destroyDynamicMesh
=========
*/
void RenderBackend::destroyDynamicMesh(const DynamicMesh& mesh) {
    destroyBuffer(mesh.vertexBuffer.buffer);
    destroyBuffer(mesh.indexBuffer);
}

/*
=========
destroyGraphicRenderPass
=========
*/
void RenderBackend::destroyGraphicPass(const GraphicPass& pass) {
    vkDestroyRenderPass(vkContext.device, pass.vulkanRenderPass, nullptr);
    vkDestroyFramebuffer(vkContext.device, pass.beginInfo.framebuffer, nullptr);
    vkDestroyDescriptorSetLayout(vkContext.device, pass.materialSetLayout, nullptr);
    vkDestroyPipelineLayout(vkContext.device, pass.pipelineLayout, nullptr);
    vkDestroyPipeline(vkContext.device, pass.pipeline, nullptr);
    vkDestroyDescriptorSetLayout(vkContext.device, pass.descriptorSetLayout, nullptr);
}

/*
=========
destroyComputeRenderPass
=========
*/
void RenderBackend::destroyComputePass(const ComputePass& pass) {
    vkDestroyPipelineLayout(vkContext.device, pass.pipelineLayout, nullptr);
    vkDestroyPipeline(vkContext.device, pass.pipeline, nullptr);
    vkDestroyDescriptorSetLayout(vkContext.device, pass.descriptorSetLayout, nullptr);
}