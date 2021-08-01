#include "pch.h"
#include "RenderBackend.h"

#include <vulkan/vulkan.h>

#include "Runtime/FrameIndex.h"
#include "SpirvReflection.h"
#include "VertexInput.h"
#include "ShaderIO.h"
#include "Utilities/GeneralUtils.h"
#include "Utilities/MathUtils.h"
#include "VulkanVertexInput.h"
#include "VulkanImageFormats.h"
#include "Runtime/Timer.h"
#include "JobSystem.h"
#include "VulkanImage.h"
#include "Runtime/Window.h"
#include "RenderPass.h"
#include "VulkanSync.h"
#include "DescriptorSetLayout.h"
#include "VulkanPipeline.h"
#include "VulkanShaderModule.h"
#include "VulkanShader.h"
#include "VulkanCommandRecording.h"
#include "VulkanSampler.h"
#include "VulkanBarrier.h"
#include "VulkanRenderPass.h"
#include "VulkanCommandBuffer.h"
#include "VulkanCommandPool.h"
#include "VulkanSurface.h"
#include "VulkanBuffer.h"
#include "VulkanFramebuffer.h"
#include "VulkanImageTransfer.h"
#include "VulkanPipelineLayout.h"
#include "VulkanDebug.h"
#include "VulkanDescriptorSet.h"

// definition of extern variable from header
RenderBackend gRenderBackend;

// vulkan uses enums, which result in a warning every time they are used
// this warning is disabled for this entire file
#pragma warning( disable : 26812) // Prefer 'enum class' over 'enum' 

const uint32_t maxTextureCount = 1000;

void RenderBackend::setup(GLFWwindow* window) {

    m_swapchainInputImageHandle.type = ImageHandleType::Swapchain;
    m_shaderFileManager.setup();

    createVulkanInstance();
    m_swapchain.surface = createSurface(window);
    pickPhysicalDevice(m_swapchain.surface);

    getQueueFamilies(vkContext.physicalDevice, &vkContext.queueFamilies, m_swapchain.surface);
    createLogicalDevice();
    initializeVulkanQueues();
    m_swapchain.surfaceFormat   = chooseSurfaceFormat(m_swapchain.surface);
    m_swapchain.minImageCount   = 2; // for double buffered VSync
    m_swapchain.vulkanHandle    = createVulkanSwapchain(m_swapchain.minImageCount, m_swapchain.surface, m_swapchain.surfaceFormat);

    const std::array<int, 2> resolution = Window::getGlfwWindowResolution(window);
    m_swapchain.images =  createSwapchainImages(
        (uint32_t)resolution[0], 
        (uint32_t)resolution[1], 
        m_swapchain.vulkanHandle, 
        m_swapchain.surfaceFormat.format);

    initVulkanDebugFunctions();
    initVulkanTimestamps();

    m_vkAllocator.create();
    m_commandPool           = createCommandPool(vkContext.queueFamilies.graphics, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    m_drawcallCommandPools  = createDrawcallCommandPools();

    m_swapchain.imageAvailable  = createSemaphore();
    m_renderFinishedSemaphore   = createSemaphore();
    m_renderFinishedFence       = createFence();

    const VkCommandPoolCreateFlagBits transientCmdPoolFlags = VkCommandPoolCreateFlagBits(VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    m_transferResources.transientCmdPool    = createCommandPool(vkContext.queueFamilies.transfer, transientCmdPoolFlags);
    m_transferResources.stagingBuffer       = createStagingBuffer();

    for (auto& resources : m_perFrameResources) {
        resources = createPerFrameResources(m_commandPool);
    }

    initGlobalTextureArrayDescriptorSetLayout();
    initGlobalTextureArrayDescriptorSet();
    m_imguiResources = createImguiRenderResources(window, m_swapchain, m_perFrameResources[0].commandBuffer);
}

void RenderBackend::shutdown() {

    waitForRenderFinished();
    m_shaderFileManager.shutdown();

    for (const Image &image : m_images) {
        destroyImageInternal(image);
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

    vkDestroyDescriptorSetLayout(vkContext.device, m_globalTextureArrayDescriporSetLayout, nullptr);

    destroySwapchain(m_swapchain);

    m_vkAllocator.destroy();
    imGuiShutdown(m_imguiResources);

    vkDestroyCommandPool(vkContext.device, m_transferResources.transientCmdPool, nullptr);
    destroyBuffer(m_transferResources.stagingBuffer);

    for (const auto& pool : m_descriptorPools) {
        vkDestroyDescriptorPool(vkContext.device, pool.vkPool, nullptr);
    }
    
    vkDestroyDescriptorPool(vkContext.device, m_globalTextureArrayDescriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(vkContext.device, m_globalDescriptorSetLayout, nullptr);

    vkDestroyCommandPool(vkContext.device, m_commandPool, nullptr);

    for (const VkCommandPool pool : m_drawcallCommandPools) {
        vkDestroyCommandPool(vkContext.device, pool, nullptr);
    }

    vkDestroySemaphore(vkContext.device, m_renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(vkContext.device, m_swapchain.imageAvailable, nullptr);

    for (auto& resources : m_perFrameResources) {
        destroyPerFrameResources(&resources);
    }

    vkDestroyFence(vkContext.device, m_renderFinishedFence, nullptr);
    vkDestroyDevice(vkContext.device, nullptr);
    destroyVulkanInstance();
}

void RenderBackend::recreateSwapchain(const uint32_t width, const uint32_t height, GLFWwindow* window) {

    waitForGpuIdle();

    destroySwapchain(m_swapchain);

    m_swapchain.surface = createSurface(window);

    // queue families must revalidate present support for new surface
    getQueueFamilies(vkContext.physicalDevice, &vkContext.queueFamilies, m_swapchain.surface);

    m_swapchain.vulkanHandle = createVulkanSwapchain(
        m_swapchain.minImageCount, 
        m_swapchain.surface, 
        m_swapchain.surfaceFormat);

    m_swapchain.images = createSwapchainImages(
        width, 
        height, 
        m_swapchain.vulkanHandle, 
        m_swapchain.surfaceFormat.format);

    destroyFramebuffers(m_imguiResources.framebuffers);
    m_imguiResources.framebuffers   = createImGuiFramebuffers(m_swapchain.images, m_imguiResources.renderPass);
    m_imguiResources.passBeginInfos = createImGuiPassBeginInfo(
        width, 
        height, 
        m_imguiResources.framebuffers, 
        m_imguiResources.renderPass);
}

void RenderBackend::waitForGPUIdle() {
    waitForGpuIdle();
}

void RenderBackend::updateShaderCode() {

    const std::vector<ComputePassShaderReloadInfo> computeShadersReloadInfos = m_shaderFileManager.reloadOutOfDateComputeShaders();
    const std::vector<GraphicPassShaderReloadInfo> graphicShadersReloadInfos = m_shaderFileManager.reloadOutOfDateGraphicShaders();

    const bool noShadersToReload = computeShadersReloadInfos.size() == 0 && graphicShadersReloadInfos.size() == 0;
    if (noShadersToReload) {
        return;
    }

    waitForGpuIdle();

    for (const ComputePassShaderReloadInfo& reloadInfo : computeShadersReloadInfos) {
        reloadComputePass(reloadInfo);
    }
    for (const GraphicPassShaderReloadInfo& reloadInfo : graphicShadersReloadInfos) {
        reloadGraphicPass(reloadInfo);
    }
}

void RenderBackend::resizeImages(const std::vector<ImageHandle>& images, const uint32_t width, const uint32_t height) {
    for (const auto imageHandle : images) {
        Image &image            = getImageRef(imageHandle);
        destroyImageInternal(image);
        image.desc.width        = width;
        image.desc.height       = height;
        image                   = createImageInternal(image.desc, Data());
    }
}

void RenderBackend::newFrame() {

    m_graphicPassExecutions.clear();
    m_computePassExecutions.clear();
    m_renderPassExecutions.clear();
    m_temporaryImages.clear();
    resetAllocatedTempImages();

    m_swapchainInputImageHandle.index = 0;

    markImGuiNewFrame();
}

void RenderBackend::prepareForDrawcallRecording() {
    allocateTemporaryImages();
    updateRenderPassDescriptorSets();

    std::vector<VkFramebuffer> &transientFramebuffers = m_perFrameResources[FrameIndex::getFrameIndexMod2()].transientFramebuffers;
    destroyFramebuffers(transientFramebuffers);
    transientFramebuffers = createGraphicPassFramebuffers(m_graphicPassExecutions);

    for (int i = 0; i < m_graphicPassExecutions.size(); i++) {
        startGraphicPassRecording(m_graphicPassExecutions[i], transientFramebuffers[i]);
    }
}

void RenderBackend::setGraphicPassExecution(const GraphicPassExecution& execution) {
    RenderPassExecutionEntry executionEntry;
    executionEntry.index = m_graphicPassExecutions.size();
    executionEntry.type  = RenderPassType::Graphic;
    m_renderPassExecutions.push_back(executionEntry);
    m_graphicPassExecutions.push_back(execution);
}

void RenderBackend::setComputePassExecution(const ComputePassExecution& execution) {
    RenderPassExecutionEntry executionEntry;
    executionEntry.index = m_computePassExecutions.size();
    executionEntry.type  = RenderPassType::Compute;
    m_renderPassExecutions.push_back(executionEntry);
    m_computePassExecutions.push_back(execution);
}

void RenderBackend::drawMeshes(
    const std::vector<MeshHandle>   meshHandles, 
    const char*                     pushConstantData, 
    const RenderPassHandle          passHandle, 
    const int                       workerIndex) {

    const GraphicPass& pass = m_renderPasses.getGraphicPassRefByHandle(passHandle);

    VkShaderStageFlags pushConstantStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    if (pass.graphicPassDesc.shaderDescriptions.geometry.has_value()) {
        pushConstantStageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    }

    const int poolCount = m_drawcallCommandPools.size();
    const int poolIndex = workerIndex + poolCount * FrameIndex::getFrameIndexMod2();
    
    const VkCommandBuffer meshCommandBuffer = pass.meshCommandBuffers[poolIndex];

    const VkDescriptorSet sets[3] = { 
        m_globalDescriptorSet, 
        pass.descriptorSets[FrameIndex::getFrameIndexMod2()], 
        m_globalTextureArrayDescriptorSet };

    vkCmdBindDescriptorSets(meshCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipelineLayout, 0, 3, sets, 0, nullptr);

    for (uint32_t i = 0; i < meshHandles.size(); i++) {

        const Mesh mesh = m_meshes[meshHandles[i].index];

        // vertex/index buffers
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(meshCommandBuffer, 0, 1, &mesh.vertexBuffer.vulkanHandle, &offset);
        vkCmdBindIndexBuffer(meshCommandBuffer, mesh.indexBuffer.vulkanHandle, offset, mesh.indexPrecision);

        const bool pushConstantDataAvailable = pass.pushConstantSize > 0;
        if (pushConstantDataAvailable) {
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
    const DescriptorPoolAllocationSizes setSizes    = getDescriptorSetAllocationSizeFromShaderLayout(layout);
    const VkDescriptorPool              pool        = findFittingDescriptorPool(setSizes);

    m_globalDescriptorSet = allocateVulkanDescriptorSet(m_globalDescriptorSetLayout, pool);
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
        waitForGpuIdle();
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
        waitForGpuIdle();
        const ComputeShaderHandle shaderHandle = pass.shaderHandle;
        destroyComputePass(pass);
        pass = createComputePassInternal(pass.computePassDesc, spirV);
        pass.shaderHandle = shaderHandle;
    }
}

void RenderBackend::renderFrame(const bool presentToScreen) {

    PerFrameResources& frameResources = m_perFrameResources[FrameIndex::getFrameIndexMod2()];

    // reset doesn't work before waiting for render finished fence
    resetTimestampQueryPool(&frameResources.timestampQueryPool);
    frameResources.timestampQueries.clear();

    resetCommandBuffer(frameResources.commandBuffer);
    beginCommandBuffer(frameResources.commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    TimestampQuery frameQuery;
    frameQuery.name         = "Frame";
    frameQuery.startQuery   = issueTimestampQuery(frameResources.commandBuffer, &frameResources.timestampQueryPool);
    frameResources.timestampQueries.push_back(frameQuery);

    const std::vector<RenderPassBarriers> barriers = createRenderPassBarriers();
    submitRenderPasses(&frameResources, barriers);

    frameResources.timestampQueries.front().endQuery = issueTimestampQuery(frameResources.commandBuffer, &frameResources.timestampQueryPool);

    // transition swapchain image to present
    auto& swapchainPresentImage = getImageRef(m_swapchainInputImageHandle);
    recordImageLayoutTransition(swapchainPresentImage, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, frameResources.commandBuffer);

    endCommandBufferRecording(frameResources.commandBuffer);

    // compute cpu time after drawcall recording, but before waiting for GPU to finish
    m_lastFrameCPUTime = Timer::getTimeFloat() - m_timeOfLastGPUSubmit;
    
    // wait for in flight frame to render so resources are avaible
    waitForFence(m_renderFinishedFence);
    resetFence(m_renderFinishedFence);

    executeDeferredBufferFillOrders();

    // submit command buffer to queue
    std::vector<VkSemaphore> submissionWaitSemaphores;
    std::vector<VkSemaphore> submissionSignalSemaphores;
    if (presentToScreen) {
        submissionWaitSemaphores    = { m_swapchain.imageAvailable };
        submissionSignalSemaphores  = { m_renderFinishedSemaphore };
    }

    submitCmdBufferToGraphicsQueue(
        frameResources.commandBuffer, 
        submissionWaitSemaphores,
        submissionSignalSemaphores,
        m_renderFinishedFence);

    if (presentToScreen) {
        presentImage(m_renderFinishedSemaphore, m_swapchain.vulkanHandle, m_swapchainInputImageHandle.index);
        glfwPollEvents();
    }

    m_timeOfLastGPUSubmit = Timer::getTimeFloat();

    // retrieve previous frame renderpass timings
    const int   previousFrameIndexMod2 = (FrameIndex::getFrameIndexMod2() + 1) % 2;
    const auto& previousFrameResources = m_perFrameResources[previousFrameIndexMod2];

    m_renderpassTimings = retrieveRenderPassTimes(
        previousFrameResources.timestampQueryPool,
        previousFrameResources.timestampQueries);
}

uint32_t RenderBackend::getImageGlobalTextureArrayIndex(const ImageHandle image) {
    return getImageRef(image).globalDescriptorSetIndex;
}

RenderPassHandle RenderBackend::createComputePass(const ComputePassDescription& desc) {

    const ComputeShaderHandle shaderHandle = m_shaderFileManager.addComputeShader(desc.shaderDescription);

    std::vector<uint32_t> spirV;
    if (!m_shaderFileManager.loadComputeShaderSpirV(shaderHandle, &spirV)) {
        std::cout << "Initial shader loading failed" << std::endl; // loadShaders provides error details
        throw;
    }

    ComputePass         pass                = createComputePassInternal(desc, spirV);
                        pass.shaderHandle   = shaderHandle;
    RenderPassHandle    passHandle          = m_renderPasses.addComputePass(pass);
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

    // create vulkan pass and handle
    GraphicPass pass            = createGraphicPassInternal(desc, spirV);
    pass.shaderHandle           = shaderHandle;
    RenderPassHandle passHandle = m_renderPasses.addGraphicPass(pass); 
    m_shaderFileManager.setGraphicPassHandle(shaderHandle, passHandle);
    return passHandle;
}

std::vector<MeshHandle> RenderBackend::createMeshes(const std::vector<MeshBinary>& meshes) {

    std::vector<MeshHandle> handles;
    for (const MeshBinary& meshData : meshes) {

        std::vector<uint32_t> bufferQueueFamilies = { vkContext.queueFamilies.graphics };

        Mesh mesh;
        mesh.indexCount = meshData.indexCount;

        // index buffer
        if (mesh.indexCount < std::numeric_limits<uint16_t>::max()) {
            mesh.indexPrecision = VK_INDEX_TYPE_UINT16;
        }
        else {
            mesh.indexPrecision = VK_INDEX_TYPE_UINT32;
        }

        const VkDeviceSize indexBufferSize = meshData.indexBuffer.size() * sizeof(uint16_t);
        mesh.indexBuffer = createBufferInternal(
            indexBufferSize, 
            bufferQueueFamilies, 
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        fillDeviceLocalBufferImmediate(
            mesh.indexBuffer, 
            Data(meshData.indexBuffer.data(), indexBufferSize), 
            m_transferResources);

        // vertex buffer
        const VkDeviceSize vertexBufferSize = meshData.vertexBuffer.size() * sizeof(uint8_t);
        mesh.vertexBuffer = createBufferInternal(
            vertexBufferSize, 
            bufferQueueFamilies,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        fillDeviceLocalBufferImmediate(
            mesh.vertexBuffer, 
            Data(meshData.vertexBuffer.data(), vertexBufferSize),
            m_transferResources);

        // store and return handle
        MeshHandle handle = { (uint32_t)m_meshes.size() };
        handles.push_back(handle);
        m_meshes.push_back(mesh);
    }
    return handles;
}

ImageHandle RenderBackend::createImage(
    const ImageDescription& desc, 
    const void*             initialData, 
    const size_t            initialDataSize) {

    const Image image = createImageInternal(desc, Data(initialData, initialDataSize));

    ImageHandle handle;
    handle.type = ImageHandleType::Default;
    const bool isFreeImageHandleAvailable = m_freeImageHandles.size() > 0;
    if (isFreeImageHandleAvailable) {
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
        vkContext.queueFamilies.transfer,
        vkContext.queueFamilies.graphics,
        vkContext.queueFamilies.compute };

    const Buffer uniformBuffer = createBufferInternal(
        desc.size, 
        queueFamilies,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (desc.initialData) {
        fillDeviceLocalBufferImmediate(uniformBuffer, Data(desc.initialData, desc.size), m_transferResources);
    }

    const UniformBufferHandle handle = { (uint32_t)m_uniformBuffers.size() };
    m_uniformBuffers.push_back(uniformBuffer);
    return handle;
}

StorageBufferHandle RenderBackend::createStorageBuffer(const StorageBufferDescription& desc) {

    const std::vector<uint32_t> queueFamilies = {
        vkContext.queueFamilies.transfer,
        vkContext.queueFamilies.graphics,
        vkContext.queueFamilies.compute };

    const Buffer storageBuffer = createBufferInternal(desc.size, 
        queueFamilies, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (desc.initialData) {
        fillDeviceLocalBufferImmediate(storageBuffer, Data(desc.initialData, desc.size), m_transferResources);
    }

    StorageBufferHandle handle = { (uint32_t)m_storageBuffers.size() };
    m_storageBuffers.push_back(storageBuffer);
    return handle;
}

SamplerHandle RenderBackend::createSampler(const SamplerDescription& desc) {

    const VkSampler     sampler = createVulkanSampler(desc);
    const SamplerHandle handle  = { (uint32_t)m_samplers.size() };
    m_samplers.push_back(sampler);
    return handle;
}

ImageHandle RenderBackend::createTemporaryImage(const ImageDescription& description) {
    ImageHandle handle;
    handle.type     = ImageHandleType::Transient;
    handle.index    = m_temporaryImages.size();

    TemporaryImage tempImage;
    tempImage.desc = description;
    m_temporaryImages.push_back(tempImage);

    return handle;
}

ImageHandle RenderBackend::getSwapchainInputImage() {
    const auto result = vkAcquireNextImageKHR(vkContext.device, m_swapchain.vulkanHandle, UINT64_MAX, m_swapchain.imageAvailable, VK_NULL_HANDLE, &m_swapchainInputImageHandle.index);
    checkVulkanResult(result);
    return m_swapchainInputImageHandle;
}

void RenderBackend::getMemoryStats(uint64_t* outAllocatedSize, uint64_t* outUsedSize) const{
    assert(outAllocatedSize != nullptr);
    assert(outUsedSize != nullptr);
    m_vkAllocator.getMemoryStats(outAllocatedSize, outUsedSize);
}

std::vector<RenderPassTime> RenderBackend::getRenderpassTimings() const {
    return m_renderpassTimings;
}

float RenderBackend::getLastFrameCPUTime() const {
    return m_lastFrameCPUTime;
}

ImageDescription RenderBackend::getImageDescription(const ImageHandle handle) {
    return getImageRef(handle).desc;
}

std::vector<RenderPassBarriers> RenderBackend::createRenderPassBarriers() {

    std::vector<RenderPassBarriers> barrierList;

    for (const RenderPassExecutionEntry executionEntry : m_renderPassExecutions) {

        const RenderPassExecution execution = getGenericRenderpassInfoFromExecutionEntry(
            executionEntry,
            m_graphicPassExecutions, 
            m_computePassExecutions);

        const RenderPassResources& resources = execution.resources;
        RenderPassBarriers barriers;

        // storage images
        for (const ImageResource& storageImage : resources.storageImages) {
            Image& image = getImageRef(storageImage.image);

            // check if any mip levels need a layout transition
            const VkImageLayout requiredLayout = VK_IMAGE_LAYOUT_GENERAL;
            bool needsLayoutTransition = false;
            for (const auto& layout : image.layoutPerMip) {
                if (layout != requiredLayout) {
                    needsLayoutTransition = true;
                }
            }

            // check if image already has a barrier
            // can happen if same image is used as two storage image when accessing different mips
            bool hasBarrierAlready = false;
            for (const auto& barrier : barriers.imageBarriers) {
                if (barrier.image == image.vulkanHandle) {
                    hasBarrierAlready = true;
                    break;
                }
            }
            
            const bool isBarrierRequired    = image.currentlyWriting    || needsLayoutTransition;
            const bool needToAddBarrier     = isBarrierRequired         && !hasBarrierAlready;
            if (needToAddBarrier) {
                const auto& layoutBarriers = createImageBarriers(
                    image, requiredLayout,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, 
                    0, 
                    (uint32_t)image.layoutPerMip.size());
                barriers.imageBarriers.insert(barriers.imageBarriers.end(), layoutBarriers.begin(), layoutBarriers.end());
            }
            image.currentlyWriting = true;
        }

        // sampled images
        for (const ImageResource& sampledImage : resources.sampledImages) {

            // use general layout if image is used as a storage image too
            bool isUsedAsStorageImage = false;
            for (const ImageResource& storageImage : resources.storageImages) {
                if (storageImage.image.index == sampledImage.image.index) {
                    isUsedAsStorageImage = true;
                    break;
                }
            }
            if (isUsedAsStorageImage) {
                continue;
            }

            Image& image = getImageRef(sampledImage.image);

            // check if any mip levels need a layout transition            
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

        // attachments
        if (executionEntry.type == RenderPassType::Graphic) {
            const GraphicPassExecution graphicExecutionInfo = m_graphicPassExecutions[executionEntry.index];

            for (const RenderTarget& target : graphicExecutionInfo.targets) {
                Image& image = getImageRef(target.image);

            // check if any mip levels need a layout transition
            const VkImageLayout requiredLayout = isVulkanDepthFormat(image.format) ?
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            bool needsLayoutTransition = false;
            for (const auto& layout : image.layoutPerMip) {
                if (layout != requiredLayout) {
                    needsLayoutTransition = true;
                }
            }

            const bool isBarrierRequired = image.currentlyWriting || needsLayoutTransition;
            if (isBarrierRequired) {
                const VkAccessFlags access = isVulkanDepthFormat(image.format) ?
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                const auto& layoutBarriers = createImageBarriers(
                    image, 
                    requiredLayout, 
                    access, 
                    0,
                    (uint32_t)image.viewPerMip.size());
                barriers.imageBarriers.insert(barriers.imageBarriers.end(), layoutBarriers.begin(), layoutBarriers.end());
            }
            image.currentlyWriting = true;
            }
        }

        // storage buffer barriers
        for (const auto& bufferResource : resources.storageBuffers) {
            StorageBufferHandle handle = bufferResource.buffer;
            Buffer& buffer = m_storageBuffers[handle.index];
            const bool needsBarrier = buffer.isBeingWritten;
            if (needsBarrier) {
                VkBufferMemoryBarrier barrier = createBufferBarrier(buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
                barriers.memoryBarriers.push_back(barrier);
            }
        
            // update writing state
            buffer.isBeingWritten = !bufferResource.readOnly;
        }
        barrierList.push_back(barriers);
    }
    return barrierList;
}

void RenderBackend::submitRenderPasses(PerFrameResources* inOutFrameResources, const std::vector<RenderPassBarriers> barriers) {

    int graphicPassIndex = 0;
    for (int i = 0; i < m_renderPassExecutions.size(); i++) {
        const RenderPassExecutionEntry& executionEntry = m_renderPassExecutions[i];

        if (executionEntry.type == RenderPassType::Graphic) {
            const VkFramebuffer f = inOutFrameResources->transientFramebuffers[graphicPassIndex];
            submitGraphicPass(m_graphicPassExecutions[executionEntry.index], barriers[i], inOutFrameResources, f);
            graphicPassIndex++;
        }
        else if (executionEntry.type == RenderPassType::Compute) {
            submitComputePass(m_computePassExecutions[executionEntry.index], barriers[i], inOutFrameResources);
        }
        else {
            std::cout << "Unknown RenderPassType\n";
        }
    }
    startDebugLabel(inOutFrameResources->commandBuffer, "ImGui");

    const std::vector<VkImageMemoryBarrier> uiBarrier = createImageBarriers(
        getImageRef(m_swapchainInputImageHandle),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, 1);
    const VkRenderPassBeginInfo& uiPassBeginInfo = m_imguiResources.passBeginInfos[m_swapchainInputImageHandle.index];

    recordImGuiRenderpass(inOutFrameResources, uiBarrier, uiPassBeginInfo);
    endDebugLabel(inOutFrameResources->commandBuffer);
}

void RenderBackend::submitGraphicPass(
    const GraphicPassExecution& execution,
    const RenderPassBarriers&   barriers, 
    PerFrameResources*          inOutFrameResources, 
    const VkFramebuffer         framebuffer) {

    GraphicPass& pass = m_renderPasses.getGraphicPassRefByHandle(execution.genericInfo.handle);
    startDebugLabel(inOutFrameResources->commandBuffer, pass.graphicPassDesc.name);

    TimestampQuery timeQuery;
    timeQuery.name = pass.graphicPassDesc.name;
    timeQuery.startQuery = issueTimestampQuery(inOutFrameResources->commandBuffer, &inOutFrameResources->timestampQueryPool);

    issueBarriersCommand(inOutFrameResources->commandBuffer, barriers.imageBarriers, barriers.memoryBarriers);

    const glm::ivec2            resolution  = getResolutionFromRenderTargets(execution.targets);
    const VkRenderPassBeginInfo beginInfo   = createRenderPassBeginInfo(
        resolution.x, 
        resolution.y, 
        pass.vulkanRenderPass, 
        framebuffer, 
        pass.clearValues);

    //prepare pass
    vkCmdBeginRenderPass(inOutFrameResources->commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    // end drawcall command buffers
    const int poolCount = m_drawcallCommandPools.size();
    for (int poolIndex = 0; poolIndex < poolCount; poolIndex++) {
        const int cmdBufferIndex = poolIndex + FrameIndex::getFrameIndexMod2() * poolCount;
        const VkCommandBuffer meshCommandBuffer = pass.meshCommandBuffers[cmdBufferIndex];

        endCommandBufferRecording(meshCommandBuffer);
    }
    //execute mesh commands
    const int cmdBufferIndexOffset = FrameIndex::getFrameIndexMod2() * poolCount;
    vkCmdExecuteCommands(inOutFrameResources->commandBuffer, poolCount, &pass.meshCommandBuffers[cmdBufferIndexOffset]);

    vkCmdEndRenderPass(inOutFrameResources->commandBuffer);

    timeQuery.endQuery = issueTimestampQuery(inOutFrameResources->commandBuffer, &inOutFrameResources->timestampQueryPool);
    inOutFrameResources->timestampQueries.push_back(timeQuery);

    endDebugLabel(inOutFrameResources->commandBuffer);
}

void RenderBackend::submitComputePass(const ComputePassExecution& execution,
    const RenderPassBarriers& barriers, PerFrameResources *inOutFrameResources) {

    ComputePass& pass = m_renderPasses.getComputePassRefByHandle(execution.genericInfo.handle);
    startDebugLabel(inOutFrameResources->commandBuffer, pass.computePassDesc.name);

    TimestampQuery timeQuery;
    timeQuery.name = pass.computePassDesc.name;
    timeQuery.startQuery = issueTimestampQuery(inOutFrameResources->commandBuffer, &inOutFrameResources->timestampQueryPool);

    issueBarriersCommand(inOutFrameResources->commandBuffer, barriers.imageBarriers, barriers.memoryBarriers);

    vkCmdBindPipeline(inOutFrameResources->commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipeline);

    const VkDescriptorSet sets[3] = { 
        m_globalDescriptorSet, 
        pass.descriptorSets[FrameIndex::getFrameIndexMod2()], 
        m_globalTextureArrayDescriptorSet };

    vkCmdBindDescriptorSets(
        inOutFrameResources->commandBuffer, 
        VK_PIPELINE_BIND_POINT_COMPUTE, 
        pass.pipelineLayout, 
        0, 
        3, 
        sets, 
        0, 
        nullptr);

    if (execution.pushConstants.size() > 0) {
        vkCmdPushConstants(
            inOutFrameResources->commandBuffer,
            pass.pipelineLayout, 
            VK_SHADER_STAGE_COMPUTE_BIT, 
            0,
            sizeof(char) * (uint32_t)execution.pushConstants.size(), 
            execution.pushConstants.data());
    }

    vkCmdDispatch(inOutFrameResources->commandBuffer, execution.dispatchCount[0], execution.dispatchCount[1], execution.dispatchCount[2]);

    timeQuery.endQuery = issueTimestampQuery(inOutFrameResources->commandBuffer, &inOutFrameResources->timestampQueryPool);
    inOutFrameResources->timestampQueries.push_back(timeQuery);

    endDebugLabel(inOutFrameResources->commandBuffer);
}

void RenderBackend::waitForRenderFinished() {
    waitForFence(m_renderFinishedFence);
}

void RenderBackend::executeDeferredBufferFillOrders() {
    for (const UniformBufferFillOrder& order : m_deferredUniformBufferFills) {
        fillDeviceLocalBufferImmediate(
            m_uniformBuffers[order.buffer.index], 
            Data(order.data.data(), order.data.size()),
            m_transferResources);
    }
    for (const StorageBufferFillOrder& order : m_deferredStorageBufferFills) {
        fillDeviceLocalBufferImmediate(
            m_storageBuffers[order.buffer.index], 
            Data(order.data.data(), order.data.size()),
            m_transferResources);
    }
    m_deferredUniformBufferFills.clear();
    m_deferredStorageBufferFills.clear();
}

std::vector<VkFramebuffer> RenderBackend::createGraphicPassFramebuffers(const std::vector<GraphicPassExecution>& execution) {
    std::vector<VkFramebuffer> framebuffers;
    for (const GraphicPassExecution& exe : execution) {
        const GraphicPass pass = m_renderPasses.getGraphicPassRefByHandle(exe.genericInfo.handle);
        if (validateRenderTargets(exe.targets)) {
            const std::vector<VkImageView>  targetViews     = getImageViewsFromRenderTargets(exe.targets);
            const glm::ivec2                resolution      = getResolutionFromRenderTargets(exe.targets);

            const VkFramebuffer newFramebuffer = createVulkanFramebuffer(
                targetViews, 
                pass.vulkanRenderPass,
                resolution.x, 
                resolution.y);
            framebuffers.push_back(newFramebuffer);
        }
        else {
            std::cerr << "Cannot create graphic pass framebuffer: invalid attachments\n";
            framebuffers.push_back(VK_NULL_HANDLE);
        }
    }
    return framebuffers;
}

std::vector<VkImageView> RenderBackend::getImageViewsFromRenderTargets(const std::vector<RenderTarget>& targets) {
    std::vector<VkImageView> views;
    for (const auto& target : targets) {
        const auto& image   = getImageRef(target.image);
        const auto view     = image.viewPerMip[target.mipLevel];
        views.push_back(view);
    }
    return views;
}

bool RenderBackend::validateRenderTargets(const std::vector<RenderTarget>& targets) {

    const std::string failureMessagePrologue = "RenderTarget validation failed: ";
    if (targets.size() == 0) {
        std::cout << failureMessagePrologue << "no attachments\n";
        return false;
    }

    glm::uvec2 resolution = getResolutionFromRenderTargets(targets);

    for (const auto attachmentDefinition : targets) {

        const Image& attachment = getImageRef(attachmentDefinition.image);
        const bool imageHasAttachmentUsageFlag = bool(attachment.desc.usageFlags | ImageUsageFlags::Attachment);

        if (!imageHasAttachmentUsageFlag) {
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

glm::uvec2 RenderBackend::getResolutionFromRenderTargets(const std::vector<RenderTarget>& targets) {
    if (targets.size() == 0) {
        return glm::uvec2(0);
    }
    const Image& firstImage = getImageRef(targets[0].image);
    return glm::uvec2(firstImage.desc.width, firstImage.desc.height);
}

Image& RenderBackend::getImageRef(const ImageHandle handle) {
    if (handle.type == ImageHandleType::Default) {
        assert(handle.index < m_images.size());
        return m_images[handle.index];
    }
    else if(handle.type == ImageHandleType::Transient){
        const int allocationIndex = m_temporaryImages[handle.index].allocationIndex;
        assert(allocationIndex < m_allocatedTempImages.size());
        return m_allocatedTempImages[allocationIndex].image;
    }
    else if (handle.type == ImageHandleType::Swapchain) {
        assert(handle.index < m_swapchain.images.size());
        return m_swapchain.images[handle.index];
    }
    else {
        std::cerr << "Unknown image handle type\n";
        return m_images.front();
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

bool isTransientImageHandle(const ImageHandle handle) {
    return handle.type == ImageHandleType::Transient;
}

void RenderBackend::allocateTemporaryImages() {

    // build a list of all temporary images and the renderpasses they belong to
    struct TempImageOccurence {
        uint32_t renderpassIndex;
        uint32_t imageIndex;
    };

    std::vector<TempImageOccurence> tempImageOccurences;
    for (uint32_t i = 0; i < m_renderPassExecutions.size(); i++) {
        const auto& executionEntry = m_renderPassExecutions[i];

        const RenderPassExecution& execution = getGenericRenderpassInfoFromExecutionEntry(
            executionEntry,
            m_graphicPassExecutions, 
            m_computePassExecutions);

        for (const auto& imageResource : execution.resources.sampledImages) {
            if (isTransientImageHandle(imageResource.image)) {
                tempImageOccurences.push_back(TempImageOccurence{ i, imageResource.image.index });
            }
        }

        for (const auto& imageResource : execution.resources.storageImages) {
            if (isTransientImageHandle(imageResource.image)) {
                tempImageOccurences.push_back(TempImageOccurence{ i, imageResource.image.index });
            }
        }

        if (executionEntry.type == RenderPassType::Graphic) {
            const auto& graphicPass = m_graphicPassExecutions[executionEntry.index];
            for (const auto& target : graphicPass.targets) {
                if (isTransientImageHandle(target.image)) {
                    tempImageOccurences.push_back(TempImageOccurence{ i, target.image.index });
                }
            }
        }
    }

    // build a list of first and last usage per temp image
    // this is used to check if matching temp images do not overlap, in which case they can share an allocated image
    struct TempImageUsage {
        uint32_t firstUse = std::numeric_limits<int>::max();
        uint32_t lastUse = 0;
    };
    std::vector<TempImageUsage> imagesUsage(m_temporaryImages.size());

    for(const auto& occurence : tempImageOccurences) {
        TempImageUsage& usage = imagesUsage[occurence.imageIndex];
        usage.firstUse  = std::min(usage.firstUse,  occurence.renderpassIndex);
        usage.lastUse   = std::max(usage.lastUse,   occurence.renderpassIndex);
    };

    // this list tracks per allocated image when their latest usage is
    // used to check if allocation usage overlaps with temp image usage
    std::vector<int> allocatedImageLatestUsedPass(m_allocatedTempImages.size(), 0);

    // actual allocation
    for (const auto& occurence : tempImageOccurences) {

        auto& tempImage = m_temporaryImages[occurence.imageIndex];

        const bool isAlreadyAllocated = tempImage.allocationIndex >= 0;
        if (isAlreadyAllocated) {
            continue;
        }

        bool                    foundAllocatedImage = false;
        const TempImageUsage&   usage               = imagesUsage[occurence.imageIndex];

        for (int allocatedImageIndex = 0; allocatedImageIndex < m_allocatedTempImages.size(); allocatedImageIndex++) {

            int&                allocatedImageLastUse       = allocatedImageLatestUsedPass[allocatedImageIndex];
            AllocatedTempImage& allocatedImage              = m_allocatedTempImages[allocatedImageIndex];
            const bool          isAllocatedImageAvailable   = allocatedImageLastUse < occurence.renderpassIndex;

            const bool requirementsMatching = imageDescriptionsMatch(tempImage.desc, allocatedImage.image.desc);
            if (isAllocatedImageAvailable && requirementsMatching) {
                tempImage.allocationIndex       = allocatedImageIndex;
                allocatedImageLastUse           = usage.lastUse;
                foundAllocatedImage             = true;
                allocatedImage.usedThisFrame    = true;
                break;
            }
        }
        if (!foundAllocatedImage) {
            std::cout << "Allocated temp image\n";

            AllocatedTempImage allocatedImage;
            allocatedImage.image            = createImageInternal(tempImage.desc, Data());
            allocatedImage.usedThisFrame    = true;

            tempImage.allocationIndex       = m_allocatedTempImages.size();
            m_allocatedTempImages.push_back(allocatedImage);
            allocatedImageLatestUsedPass.push_back(usage.lastUse);
        }
    }
}

void RenderBackend::resetAllocatedTempImages() {
    for (int i = 0; i < m_allocatedTempImages.size(); i++) {
        if (!m_allocatedTempImages[i].usedThisFrame) {
            // delete unused image
            std::swap(m_allocatedTempImages.back(), m_allocatedTempImages[i]);
            waitForGpuIdle(); // FIXME: don't use wait idle, use deferred destruction queue instead
            destroyImageInternal(m_allocatedTempImages.back().image);
            m_allocatedTempImages.pop_back();
            std::cout << "Deleted unused temp image\n";
        }
        else {
            m_allocatedTempImages[i].usedThisFrame = false;
        }
    }
}

void RenderBackend::updateRenderPassDescriptorSets() {
    for (const auto& executionEntry : m_renderPassExecutions) {
        if (executionEntry.type == RenderPassType::Graphic) {
            const auto& execution = m_graphicPassExecutions[executionEntry.index];
            const VkDescriptorSet descriptorSet = m_renderPasses.getGraphicPassRefByHandle(execution.genericInfo.handle).descriptorSets[FrameIndex::getFrameIndexMod2()];
            updateDescriptorSet(descriptorSet, execution.genericInfo.resources);
        }
        else {
            const auto& execution = m_computePassExecutions[executionEntry.index];
            const VkDescriptorSet descriptorSet = m_renderPasses.getComputePassRefByHandle(execution.genericInfo.handle).descriptorSets[FrameIndex::getFrameIndexMod2()];
            updateDescriptorSet(descriptorSet, execution.genericInfo.resources);
        }
    }
}

void RenderBackend::startGraphicPassRecording(
    const GraphicPassExecution& execution, 
    const VkFramebuffer         framebuffer) {

    const RenderPassHandle  passHandle  = execution.genericInfo.handle;
    const GraphicPass       pass        = m_renderPasses.getGraphicPassRefByHandle(passHandle);

    const auto inheritanceInfo = createCommandBufferInheritanceInfo(pass.vulkanRenderPass, framebuffer);

    const int poolCount = m_drawcallCommandPools.size();
    for (int cmdBufferIndex = 0; cmdBufferIndex < poolCount; cmdBufferIndex++) {

        const int               finalIndex          = cmdBufferIndex + FrameIndex::getFrameIndexMod2() * poolCount;
        const VkCommandBuffer   meshCommandBuffer   = pass.meshCommandBuffers[finalIndex];

        resetCommandBuffer(meshCommandBuffer);
        beginCommandBuffer(meshCommandBuffer, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
            &inheritanceInfo);

        // prepare state for drawcall recording
        vkCmdBindPipeline(meshCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipeline);

        const glm::vec2 resolution = (glm::vec2)getResolutionFromRenderTargets(execution.targets);
        recordSetViewportCommand(meshCommandBuffer, resolution.x, resolution.y);
        recordSetScissorCommand(meshCommandBuffer, resolution.x, resolution.y);
    }
}

Image RenderBackend::createImageInternal(const ImageDescription& desc, const Data& initialData) {

    const uint32_t  mipCount            = computeImageMipCount(desc);
    const bool      bFillImageWithData  = initialData.size > 0;

    Image image;
    image.desc          = desc;
    image.format        = imageFormatToVulkanFormat(desc.format);;
    image.layoutPerMip  = createInitialImageLayouts(mipCount);
    image.vulkanHandle  = createVulkanImage(desc, bFillImageWithData);
    image.memory        = allocateAndBindImageMemory(image.vulkanHandle, &m_vkAllocator);
    image.viewPerMip    = createImageViews(image, mipCount);

    if (bFillImageWithData) {
        transferDataIntoImageImmediate(image, initialData, m_transferResources);
    }
    if (desc.autoCreateMips) {
        generateMipChainImmediate(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_transferResources.transientCmdPool);
    }

    // most textures with sampled usage are used by the material system
    // the material systems assumes the read_only_optimal layout
    // if no mips are generated the layout will still be transfer_dst or undefined
    // to avoid issues all sampled images without mip generation are manually transitioned to read_only_optimal
    const bool manualLayoutTransitionRequired = bool(desc.usageFlags & ImageUsageFlags::Sampled) && !desc.autoCreateMips;
    if (manualLayoutTransitionRequired) {
        imageLayoutTransitionImmediate(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_transferResources.transientCmdPool);
    }

    const bool imageCanBeSampled = bool(desc.usageFlags & ImageUsageFlags::Sampled);
    if (imageCanBeSampled) {
        addImageToGlobalDescriptorSetLayout(image);
    }
    return image;
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

Buffer RenderBackend::createBufferInternal(const VkDeviceSize size, const std::vector<uint32_t>& queueFamilies, const VkBufferUsageFlags usage, const uint32_t memoryFlags) {

    const std::vector<uint32_t> uniqueQueueFamilies = makeUniqueQueueFamilyList(queueFamilies);

    Buffer buffer;
    buffer.size         = size;
    buffer.vulkanHandle = createVulkanBuffer(size, usage, uniqueQueueFamilies);
    buffer.memory       = allocateAndBindBufferMemory(buffer.vulkanHandle, memoryFlags, m_vkAllocator);

    return buffer;
}

VkDescriptorPool RenderBackend::findFittingDescriptorPool(const DescriptorPoolAllocationSizes& requiredSizes){

    VkDescriptorPool fittingPool = VK_NULL_HANDLE;

    for (auto& pool : m_descriptorPools) {
        if (hasDescriptorPoolEnoughFreeAllocations(pool, requiredSizes)) {
            fittingPool = pool.vkPool;
            pool.freeAllocations = subtractDescriptorPoolSizes(pool.freeAllocations, requiredSizes);
        }
    }
    const bool noFittingPoolFound = fittingPool == VK_NULL_HANDLE;
    if (noFittingPoolFound) {
        m_descriptorPools.push_back(createDescriptorPool());
        fittingPool = m_descriptorPools.back().vkPool;
    }
    return fittingPool;
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

    // buffer and images are given via pointer
    // stored in vector to keep pointer valid
    // resize first to avoid push_back invalidating pointers
    uint32_t imageInfoIndex = 0;
    uint32_t bufferInfoIndex = 0;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;

    imageInfos.resize(resources.samplers.size() + resources.storageImages.size() + resources.sampledImages.size());
    bufferInfos.resize(resources.uniformBuffers.size() + resources.storageBuffers.size());

    // samplers
    for (const auto& resource : resources.samplers) {
        VkDescriptorImageInfo samplerInfo;
        samplerInfo.sampler = m_samplers[resource.sampler.index];
        imageInfos[imageInfoIndex] = samplerInfo;
        const auto writeSet = createWriteDescriptorSet(resource.binding, VK_DESCRIPTOR_TYPE_SAMPLER, nullptr, &imageInfos[imageInfoIndex]);
        imageInfoIndex++;
        descriptorInfos.push_back(writeSet);
    }

    // sampled images
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

    // storage images
    for (const auto& resource : resources.storageImages) {
        VkDescriptorImageInfo imageInfo;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = getImageRef(resource.image).viewPerMip[resource.mipLevel];
        imageInfos[imageInfoIndex] = imageInfo;
        const auto writeSet = createWriteDescriptorSet(resource.binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr, &imageInfos[imageInfoIndex]);
        imageInfoIndex++;
        descriptorInfos.push_back(writeSet);
    }

    // uniform buffer
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

    // storage buffer
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

ComputePass RenderBackend::createComputePassInternal(const ComputePassDescription& desc, const std::vector<uint32_t>& spirV) {

    ComputePass pass;
    pass.computePassDesc = desc;

    const VkShaderStageFlagBits stageFlags  = VK_SHADER_STAGE_COMPUTE_BIT;
    const VkShaderModule        module      = createShaderModule(spirV);
    const ShaderReflection      reflection  = performComputeShaderReflection(spirV);

    pass.descriptorSetLayout = createDescriptorSetLayout(reflection.shaderLayout);

    ShaderSpecialisationStructs specialisationStructs;
    createShaderSpecialisationStructs(desc.shaderDescription.specialisationConstants, &specialisationStructs);
    VkPipelineShaderStageCreateInfo shaderStageInfo = createPipelineShaderStageInfos(module, stageFlags, &specialisationStructs.info);

    const VkDescriptorSetLayout setLayouts[3] = {
        m_globalDescriptorSetLayout,
        pass.descriptorSetLayout,
        m_globalTextureArrayDescriporSetLayout };

    pass.pipelineLayout     = createPipelineLayout(setLayouts, reflection.pushConstantByteSize, stageFlags);
    pass.pushConstantSize   = reflection.pushConstantByteSize;
    pass.pipeline           = createVulkanComputePipeline(pass.pipelineLayout, shaderStageInfo);

    vkDestroyShaderModule(vkContext.device, module, nullptr);

    const DescriptorPoolAllocationSizes setSizes = getDescriptorSetAllocationSizeFromShaderLayout(reflection.shaderLayout);
    for (int i = 0; i < 2; i++) {
        const VkDescriptorPool pool = findFittingDescriptorPool(setSizes);
        pass.descriptorSets[i] = allocateVulkanDescriptorSet(pass.descriptorSetLayout, pool);
    }

    return pass;
}

void RenderBackend::reloadComputePass(const ComputePassShaderReloadInfo& reloadInfo) {
    ComputePass& pass = m_renderPasses.getComputePassRefByHandle(reloadInfo.renderpass);
    ComputeShaderHandle shaderHandle = pass.shaderHandle;
    destroyComputePass(pass);
    pass = createComputePassInternal(pass.computePassDesc, reloadInfo.spirV);
    pass.shaderHandle = shaderHandle;
}

void RenderBackend::reloadGraphicPass(const GraphicPassShaderReloadInfo& reloadInfo) {
    GraphicPass& pass = m_renderPasses.getGraphicPassRefByHandle(reloadInfo.renderpass);
    const GraphicShadersHandle shaderHandle = pass.shaderHandle;
    destroyGraphicPass(pass);
    pass = createGraphicPassInternal(pass.graphicPassDesc, reloadInfo.spirV);
    pass.shaderHandle = shaderHandle;
}

Buffer RenderBackend::createStagingBuffer() {

    VkDeviceSize stagingBufferSize = 1048576; // 1mb

    const auto stagingBufferUsageFlags  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const auto stagingBufferMemoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    const std::vector<uint32_t> stagingBufferQueueFamilies = { vkContext.queueFamilies.transfer };
    return createBufferInternal(
        stagingBufferSize,
        stagingBufferQueueFamilies,
        stagingBufferUsageFlags,
        stagingBufferMemoryFlags);
}

void RenderBackend::initGlobalTextureArrayDescriptorSetLayout() {

    VkDescriptorSetLayoutBinding textureArrayBinding;
    textureArrayBinding.binding             = 0;
    textureArrayBinding.descriptorType      = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    textureArrayBinding.descriptorCount     = maxTextureCount;
    textureArrayBinding.stageFlags          = VK_SHADER_STAGE_ALL;
    textureArrayBinding.pImmutableSamplers  = nullptr;

    const VkDescriptorBindingFlags flags = 
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo flagInfo;
    flagInfo.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flagInfo.pNext          = nullptr;
    flagInfo.bindingCount   = 1;
    flagInfo.pBindingFlags  = &flags;

    VkDescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext        = &flagInfo;
    layoutInfo.flags        = 0;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &textureArrayBinding;

    const VkResult result = vkCreateDescriptorSetLayout(vkContext.device, &layoutInfo, nullptr, &m_globalTextureArrayDescriporSetLayout);
    checkVulkanResult(result);
}

void RenderBackend::setGlobalTextureArrayDescriptorSetTexture(const VkImageView imageView, const uint32_t index) {

    VkDescriptorImageInfo imageInfo;
    imageInfo.imageView     = imageView;
    imageInfo.imageLayout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write;
    write.sType             = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext             = nullptr;
    write.dstSet            = m_globalTextureArrayDescriptorSet;
    write.dstBinding        = 0;
    write.dstArrayElement   = index;
    write.descriptorCount   = 1;
    write.descriptorType    = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo        = &imageInfo;
    write.pBufferInfo       = nullptr;
    write.pTexelBufferView  = nullptr;

    vkUpdateDescriptorSets(vkContext.device, 1, &write, 0, nullptr);
}

void RenderBackend::initGlobalTextureArrayDescriptorSet() {
    DescriptorPoolAllocationSizes layoutSizes;
    layoutSizes.imageSampled = maxTextureCount;

    VkDescriptorPoolSize poolSize;
    poolSize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSize.descriptorCount = maxTextureCount;

    VkDescriptorPoolCreateInfo poolInfo;
    poolInfo.sType          = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext          = nullptr;
    poolInfo.flags          = 0;
    poolInfo.maxSets        = 1;
    poolInfo.poolSizeCount  = 1;
    poolInfo.pPoolSizes     = &poolSize;

    VkResult result = vkCreateDescriptorPool(vkContext.device, &poolInfo, nullptr, &m_globalTextureArrayDescriptorPool);
    checkVulkanResult(result);

    VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo;
    variableDescriptorCountInfo.sType               = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variableDescriptorCountInfo.pNext               = nullptr;
    variableDescriptorCountInfo.descriptorSetCount  = 1;
    variableDescriptorCountInfo.pDescriptorCounts   = &maxTextureCount;

    VkDescriptorSetAllocateInfo setInfo;
    setInfo.sType               = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setInfo.pNext               = &variableDescriptorCountInfo;
    setInfo.descriptorPool      = m_globalTextureArrayDescriptorPool;
    setInfo.descriptorSetCount  = 1;
    setInfo.pSetLayouts         = &m_globalTextureArrayDescriporSetLayout;

    result = vkAllocateDescriptorSets(vkContext.device, &setInfo, &m_globalTextureArrayDescriptorSet);
    checkVulkanResult(result);
}

GraphicPass RenderBackend::createGraphicPassInternal(const GraphicPassDescription &desc, const GraphicPassShaderSpirV &spirV) {

    GraphicPass pass;
    pass.graphicPassDesc                            = desc;
    pass.meshCommandBuffers                         = createGraphicPassMeshCommandBuffers(m_drawcallCommandPools, 2);
    const GraphicPassShaderModules  shaderModules   = createGraphicPassShaderModules(spirV);

    GraphicPassSpecialisationStructs specialisationStructs;
    const auto shaderStages = createGraphicPipelineShaderCreateInfo(
        desc.shaderDescriptions, 
        shaderModules, 
        &specialisationStructs);

    const ShaderReflection      reflection                  = performShaderReflection(spirV);
    const VkShaderStageFlags    pipelineShaderStageFlags    = getGraphicPassShaderStageFlags(shaderModules);
    pass.descriptorSetLayout                                = createDescriptorSetLayout(reflection.shaderLayout);

    const VkDescriptorSetLayout setLayouts[3] = {
        m_globalDescriptorSetLayout,
        pass.descriptorSetLayout,
        m_globalTextureArrayDescriporSetLayout };

    pass.pipelineLayout     = createPipelineLayout(setLayouts, reflection.pushConstantByteSize, pipelineShaderStageFlags);
    pass.pushConstantSize   = reflection.pushConstantByteSize;
    pass.clearValues        = createGraphicPassClearValues(desc.attachments);
    pass.vulkanRenderPass   = createVulkanRenderPass(desc.attachments);
    pass.pipeline           = createVulkanGraphicsPipeline(desc, pass.pipelineLayout, pass.vulkanRenderPass, shaderStages, reflection);

    destroyGraphicPassShaderModules(shaderModules);

    const DescriptorPoolAllocationSizes setSizes = getDescriptorSetAllocationSizeFromShaderLayout(reflection.shaderLayout);
    for (int i = 0; i < 2; i++) {
        const VkDescriptorPool pool = findFittingDescriptorPool(setSizes);
        pass.descriptorSets[i]      = allocateVulkanDescriptorSet(pass.descriptorSetLayout, pool);
    }

    return pass;
}

void RenderBackend::destroyImage(const ImageHandle handle) {
    m_freeImageHandles.push_back(handle);
    const Image& image = getImageRef(handle);
    destroyImageInternal(image);
}

void RenderBackend::destroyImageInternal(const Image& image) {
    const bool isSampled = bool(image.desc.usageFlags & ImageUsageFlags::Sampled);
    if (isSampled) {
        m_globalTextureArrayDescriptorSetFreeTextureIndices.push_back(image.globalDescriptorSetIndex);
    }
    destroyImageViews(image.viewPerMip);
    vkDestroyImage(vkContext.device, image.vulkanHandle, nullptr);
}

void RenderBackend::destroyBuffer(const Buffer& buffer) {
    vkDestroyBuffer(vkContext.device, buffer.vulkanHandle, nullptr);
    m_vkAllocator.free(buffer.memory);
}

void RenderBackend::destroyMesh(const Mesh& mesh) {
    destroyBuffer(mesh.vertexBuffer);
    destroyBuffer(mesh.indexBuffer);
}