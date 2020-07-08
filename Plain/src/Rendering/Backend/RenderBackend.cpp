#include "pch.h"
#include "RenderBackend.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vulkan/vulkan.h>

#include "SpirvReflection.h"
#include "VertexInput.h"
#include "ShaderIO.h"
#include "Utilities/GeneralUtils.h"

#include <imgui/imgui.h>
#include <imgui/examples/imgui_impl_glfw.h>
#include <imgui/examples/imgui_impl_vulkan.h>

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
    getQueueFamilies(m_context.physicalDevice, &m_context.queueFamilies);
    createLogicalDevice();

    vkGetDeviceQueue(m_context.device, m_context.queueFamilies.graphicsQueueIndex, 0, &m_context.graphicQueue);
    vkGetDeviceQueue(m_context.device, m_context.queueFamilies.presentationQueueIndex, 0, &m_context.presentQueue);
    vkGetDeviceQueue(m_context.device, m_context.queueFamilies.transferQueueFamilyIndex, 0, &m_context.transferQueue);
    vkGetDeviceQueue(m_context.device, m_context.queueFamilies.computeQueueIndex, 0, &m_context.computeQueue);

    chooseSurfaceFormat();
    createSwapChain();

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    getSwapchainImages((uint32_t)width, (uint32_t)height);

    m_commandPool = createCommandPool(m_context.queueFamilies.graphicsQueueIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    m_transientCommandPool = createCommandPool(m_context.queueFamilies.transferQueueFamilyIndex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    m_descriptorPool = createDescriptorPool(100);

    m_swapchain.imageAvaible = createSemaphore();
    m_renderFinished = createSemaphore();
    m_imageInFlight = createFence();

    /*
    create common descriptor set layouts
    */
    ShaderLayout globalLayout;
    globalLayout.uniformBufferBindings.push_back(0);
    m_globalDescriptorSetLayout = createDescriptorSetLayout(globalLayout);

    m_commandBuffer = allocateCommandBuffer();

    /*
    create global uniform buffer
    */
    std::vector<uint32_t> queueFamilies = { m_context.queueFamilies.graphicsQueueIndex, m_context.queueFamilies.computeQueueIndex };
    GlobalShaderInfo defaultInfo;
    BufferDescription globalShaderBufferDesc;
    globalShaderBufferDesc.size = sizeof(GlobalShaderInfo);
    globalShaderBufferDesc.type = BufferType::Uniform;
    globalShaderBufferDesc.initialData = &defaultInfo;
    m_globalShaderInfoBuffer = createUniformBuffer(globalShaderBufferDesc);

    /*
    create global info descriptor set
    */
    RenderPassResources globalResources;
    UniformBufferResource globalBufferResource(m_globalShaderInfoBuffer, true, 0);
    globalResources.uniformBuffers.push_back(globalBufferResource);
    m_globalDescriptorSet = allocateDescriptorSet(m_globalDescriptorSetLayout);
    updateDescriptorSet(m_globalDescriptorSet, globalResources);

    /*
    imgui
    */
    setupImgui(window);

    /*
    create swapchain copy pass
    */
    ComputePassDescription copyPass;
    copyPass.shaderPath = "imageCopy.comp";
    m_swapchain.copyToSwapchainPass = createComputePass(copyPass);
}

/*
=========
teardown
=========
*/
void RenderBackend::teardown() {

    waitForRenderFinished();

    if (m_useValidationLayers) {
        PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
            reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>
            (vkGetInstanceProcAddr(m_context.vulkanInstance, "vkDestroyDebugReportCallbackEXT"));
        vkDestroyDebugReportCallbackEXT(m_context.vulkanInstance, m_debugCallback, nullptr);
    }

    //destroy resources
    for (ImageHandle i = 0; i < m_images.size(); i++) {
        destroyImage(i);
    }
    for (const auto& pass : m_renderPasses) {
        destroyRenderPass(pass);
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
        vkDestroySampler(m_context.device, sampler, nullptr);
    }

    //destroy swapchain
    vkDestroySwapchainKHR(m_context.device, m_swapchain.vulkanHandle, nullptr);
    vkDestroySurfaceKHR(m_context.vulkanInstance, m_swapchain.surface, nullptr);

    /*
    destroy ui
    */
    for (const auto& framebuffer : m_ui.framebuffers) {
        vkDestroyFramebuffer(m_context.device, framebuffer, nullptr);
    }
    vkDestroyRenderPass(m_context.device, m_ui.renderPass, nullptr);
    ImGui_ImplVulkan_Shutdown();

    vkDestroyDescriptorPool(m_context.device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_context.device, m_globalDescriptorSetLayout, nullptr);

    vkDestroyCommandPool(m_context.device, m_commandPool, nullptr);
    vkDestroyCommandPool(m_context.device, m_transientCommandPool, nullptr);

    vkDestroySemaphore(m_context.device, m_renderFinished, nullptr);
    vkDestroySemaphore(m_context.device, m_swapchain.imageAvaible, nullptr);

    vkDestroyFence(m_context.device, m_imageInFlight, nullptr);
    vkDestroyDevice(m_context.device, nullptr);
    vkDestroyInstance(m_context.vulkanInstance, nullptr);
}

/*
=========
recreateSwapchain
=========
*/
void RenderBackend::recreateSwapchain(const uint32_t width, const uint32_t height, GLFWwindow* window) {

    vkDeviceWaitIdle(m_context.device);

    /*
    destroy swapchain and views
    */
    for (const auto& imageHandle : m_swapchain.imageHandles) {
        destroyImage(imageHandle);
    }
    vkDestroySwapchainKHR(m_context.device, m_swapchain.vulkanHandle, nullptr);
    vkDestroySurfaceKHR(m_context.vulkanInstance, m_swapchain.surface, nullptr);
    
    /*
    recreate
    */
    createSurface(window);
    /*
    queue families must revalidate present support for new surface
    */
    getQueueFamilies(m_context.physicalDevice, &m_context.queueFamilies);
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
        vkDestroyFramebuffer(m_context.device, oldBuffer, nullptr);

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
void RenderBackend::reloadShaders() {
    vkDeviceWaitIdle(m_context.device);
    std::cout << "Reloaded Shaders\n" << std::endl;

    for (uint32_t i = 0; i < m_renderPasses.size(); i++) {
        destroyRenderPass(m_renderPasses[i]);
        if (m_renderPasses[i].isGraphicPass) {
            assert(m_renderPasses[i].graphicPassDesc.has_value());
            m_renderPasses[i] = createGraphicPassInternal(m_renderPasses[i].graphicPassDesc.value());
        }
        else {
            assert(m_renderPasses[i].computePassDesc.has_value());
            m_renderPasses[i] = createComputePassInternal(m_renderPasses[i].computePassDesc.value());
        }
    }
}

/*
=========
newFrame
=========
*/
void RenderBackend::resizeImages(const std::vector<ImageHandle>& images, const uint32_t width, const uint32_t height) {

    /*
    recreate image
    */
    for (const auto image : images) {
        m_images[image].desc.width = width;
        m_images[image].desc.height = height;
        const auto imageDesc = m_images[image].desc;
        destroyImage(image);
        ImageHandle newHandle = createImage(imageDesc);
        assert(newHandle == image);
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

    for (auto& pass : m_renderPasses) {
        if (!pass.isGraphicPass) {
            continue;
        }
        bool mustBeResized = false;
        for (const auto& image : images) {
            if (vectorContains(pass.attachments, image)) {
                mustBeResized = true;
            }
        }
        if (mustBeResized) {
            vkDestroyFramebuffer(m_context.device, pass.beginInfo.framebuffer, nullptr);
            pass.beginInfo.framebuffer = createFramebuffer(pass.vulkanRenderPass, extent, pass.attachmentDescriptions);
            pass.beginInfo.renderArea = rect;
            pass.viewport.width = width;
            pass.viewport.height = height;
            pass.scissor.extent = extent;
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
drawMesh
=========
*/
void RenderBackend::drawMesh(const MeshHandle meshHandle, const std::vector<RenderPassHandle>& passes, const glm::mat4& modelMatrix) {

    const auto mesh = m_meshes[meshHandle];
    MeshRenderCommand command;
    command.indexBuffer = mesh.indexBuffer.vulkanHandle;
    command.indexCount = mesh.indexCount;
    command.modelMatrix = modelMatrix;

    for (const auto passHandle : passes) {
        auto& pass = m_renderPasses[passHandle];
        for (const auto& material : mesh.materials) {
            if (material.flags == pass.materialFeatures) {
                command.materialSet = material.descriptorSet;
            }
        }
        for (const auto& vertexBuffer : mesh.vertexBuffers) {
            if (vertexBuffer.flags == pass.vertexInputFlags) {
                command.vertexBuffer = vertexBuffer.vertexBuffer.vulkanHandle;
            }
        }
        pass.currentMeshRenderCommands.push_back(command);
    }
}

/*
=========
setViewProjectionMatrix
=========
*/
void RenderBackend::setViewProjectionMatrix(const glm::mat4& viewProjection, const RenderPassHandle pass) {
    m_renderPasses[pass].viewProjectionMatrix = viewProjection;
}

/*
=========
setGlobalShaderInfo
=========
*/
void RenderBackend::setGlobalShaderInfo(const GlobalShaderInfo& info) {
    const auto buffer = m_uniformBuffers[m_globalShaderInfoBuffer];
    fillBuffer(buffer, &info, sizeof(info));
}

/*
=========
renderFrame
=========
*/
void RenderBackend::renderFrame() {

    vkWaitForFences(m_context.device, 1, &m_imageInFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(m_context.device, 1, &m_imageInFlight);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(m_context.device, m_swapchain.vulkanHandle, UINT64_MAX, m_swapchain.imageAvaible, VK_NULL_HANDLE, &imageIndex);
    prepareRenderPasses(m_swapchain.imageHandles[imageIndex]);

    //record command buffer
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;

    vkResetCommandBuffer(m_commandBuffer, 0);
    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
   
    for (const auto& execution : m_renderPassInternalExecutions) {
        submitRenderPass(execution);
    }

    /*
    imgui
    */
    ImGui::Render();

    vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1, m_ui.barriers.data());

    vkCmdBeginRenderPass(m_commandBuffer, &m_ui.passBeginInfos[imageIndex], VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffer);
    vkCmdEndRenderPass(m_commandBuffer);

    /*
    transition swapchain image to present
    */
    auto& swapchainPresentImage = m_images[m_swapchain.imageHandles[imageIndex]];
    const auto& transitionToPresentBarrier = createImageBarriers(swapchainPresentImage, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0, 1);
    barriersCommand(m_commandBuffer, transitionToPresentBarrier, std::vector<VkBufferMemoryBarrier> {});

    vkEndCommandBuffer(m_commandBuffer);

    //submit 
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = nullptr;
    submit.waitSemaphoreCount = 1;

    VkSemaphore imageAvaible = m_swapchain.imageAvaible;
    submit.pWaitSemaphores = &imageAvaible;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submit.pWaitDstStageMask = &waitStage;

    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_commandBuffer;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &m_renderFinished;

    vkQueueSubmit(m_context.graphicQueue, 1, &submit, m_imageInFlight);

    presentImage(imageIndex, m_renderFinished);
    glfwPollEvents();

    /*
    cleanup
    */
    for (auto& pass : m_renderPasses) {
        pass.currentMeshRenderCommands.clear();
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

    RenderPass pass = createComputePassInternal(desc);
    RenderPassHandle handle = m_renderPasses.size();
    m_renderPasses.push_back(pass);
    return handle;
}

/*
=========
createGraphicPass
=========
*/
RenderPassHandle RenderBackend::createGraphicPass(const GraphicPassDescription& desc) {

    RenderPass pass = createGraphicPassInternal(desc);
    RenderPassHandle handle = m_renderPasses.size();
    m_renderPasses.push_back(pass);
    
    return handle;
}

/*
=========
createMesh
=========
*/
MeshHandle RenderBackend::createMesh(const MeshData& data, const std::vector<RenderPassHandle>& passes) {

    std::vector<uint32_t> queueFamilies = { m_context.queueFamilies.graphicsQueueIndex };
    Mesh mesh;
    mesh.indexCount = data.indices.size();

    /*
    index buffer
    */
    VkDeviceSize indexDataSize = data.indices.size() * sizeof(uint32_t);
    mesh.indexBuffer = createBufferInternal(indexDataSize, queueFamilies, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    fillBuffer(mesh.indexBuffer, data.indices.data(), indexDataSize);

    /*
    vertex buffer per pass
    */
    for (const auto passHandle : passes) {

        const auto& pass = m_renderPasses[passHandle];

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
        std::vector<float> vertexData;

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

        /*
        fill in vertex data
        */
        for (size_t i = 0; i < nVertices; i++) {
            if (pass.vertexInputFlags & VERTEX_INPUT_POSITION_BIT) {
                vertexData.push_back(data.positions[i].x);
                vertexData.push_back(data.positions[i].y);
                vertexData.push_back(data.positions[i].z);
            }
            if (pass.vertexInputFlags & VERTEX_INPUT_UV_BIT) {
                vertexData.push_back(data.uvs[i].x);
                vertexData.push_back(data.uvs[i].y);
            }
            if (pass.vertexInputFlags & VERTEX_INPUT_NORMAL_BIT) {
                vertexData.push_back(data.normals[i].x);
                vertexData.push_back(data.normals[i].y);
                vertexData.push_back(data.normals[i].z);
            }
            if (pass.vertexInputFlags & VERTEX_INPUT_TANGENT_BIT) {
                vertexData.push_back(data.tangents[i].x);
                vertexData.push_back(data.tangents[i].y);
                vertexData.push_back(data.tangents[i].z);
            }
            if (pass.vertexInputFlags & VERTEX_INPUT_BITANGENT_BIT) {
                vertexData.push_back(data.bitangents[i].x);
                vertexData.push_back(data.bitangents[i].y);
                vertexData.push_back(data.bitangents[i].z);
            }
        }

        /*
        create vertex buffer
        */
        MeshVertexBuffer buffer;
        buffer.flags = pass.vertexInputFlags;
        VkDeviceSize vertexDataSize = vertexData.size() * sizeof(float);

        buffer.vertexBuffer = createBufferInternal(vertexDataSize, queueFamilies, 
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        fillBuffer(buffer.vertexBuffer, vertexData.data(), vertexDataSize);

        mesh.vertexBuffers.push_back(buffer);
    }

    /*
    material per pass
    */
    std::optional<ImageHandle> albedoTexture;
    std::optional<ImageHandle> normalTexture;
    std::optional<ImageHandle> metalicTexture;
    std::optional<ImageHandle> roughnessTexture;

    std::optional<SamplerHandle> albedoSampler;
    std::optional<SamplerHandle> normalSampler;
    std::optional<SamplerHandle> metalicSampler;
    std::optional<SamplerHandle> roughnessSampler;

    for (const auto passHandle : passes) {

        const auto& pass = m_renderPasses[passHandle];

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

        if (pass.materialFeatures & MATERIAL_FEATURE_FLAG_ALBEDO_TEXTURE) {
            if (!albedoTexture.has_value()) {
                albedoTexture = createImage(data.material.diffuseTexture);
            }
            if (!albedoSampler.has_value()) {
                SamplerDescription albedoSamplerDesc;
                albedoSamplerDesc.interpolation = SamplerInterpolation::Linear;
                albedoSamplerDesc.wrapping = SamplerWrapping::Repeat;
                albedoSamplerDesc.maxMip = m_images[albedoTexture.value()].viewPerMip.size();
                albedoSamplerDesc.useAnisotropy = true;
                albedoSampler = createSampler(albedoSamplerDesc);
            }
            const auto albedoTextureResource = ImageResource(
                albedoTexture.value(),
                0,
                4);

            const auto albedoSamplerResource = SamplerResource(
                albedoSampler.value(), 
                0);

            resources.sampledImages.push_back(albedoTextureResource);
            resources.samplers.push_back(albedoSamplerResource);
        }
        if (pass.materialFeatures & MATERIAL_FEATURE_FLAG_NORMAL_TEXTURE) {
            if (!normalTexture.has_value()) {
                normalTexture = createImage(data.material.normalTexture);
            }
            if (!normalSampler.has_value()) {
                SamplerDescription normalSamplerDesc;
                normalSamplerDesc.interpolation = SamplerInterpolation::Linear;
                normalSamplerDesc.wrapping = SamplerWrapping::Repeat;
                normalSamplerDesc.maxMip = m_images[normalTexture.value()].viewPerMip.size();
                normalSamplerDesc.useAnisotropy = true;
                normalSampler = createSampler(normalSamplerDesc);
            }
            const auto normalTextureResource = ImageResource(
                normalTexture.value(), 
                0, 
                5);

            const auto normalSamplerResource = SamplerResource(
                normalSampler.value(),
                1);

            resources.sampledImages.push_back(normalTextureResource);
            resources.samplers.push_back(normalSamplerResource);
        }
        if (pass.materialFeatures & MATERIAL_FEATURE_FLAG_METALIC_TEXTURE) {
            if (!metalicTexture.has_value()) {
                metalicTexture = createImage(data.material.metalicTexture);
            }
            if (!metalicSampler.has_value()) {
                SamplerDescription metalicSamplerDesc;
                metalicSamplerDesc.interpolation = SamplerInterpolation::Linear;
                metalicSamplerDesc.wrapping = SamplerWrapping::Repeat;
                metalicSamplerDesc.maxMip = m_images[normalTexture.value()].viewPerMip.size();
                metalicSamplerDesc.useAnisotropy = true;
                metalicSampler = createSampler(metalicSamplerDesc);
            }
            const auto metalicTextureResource = ImageResource(
                metalicTexture.value(), 
                0, 
                6);

            const auto metalicSamplerResource = SamplerResource(
                metalicSampler.value(), 
                2);

            resources.sampledImages.push_back(metalicTextureResource);
            resources.samplers.push_back(metalicSamplerResource);
        }
        if (pass.materialFeatures & MATERIAL_FEATURE_FLAG_ROUGHNESS_TEXTURE) {
            if (!roughnessTexture.has_value()) {
                roughnessTexture = createImage(data.material.roughnessTexture);
            }
            if (!roughnessSampler.has_value()) {
                SamplerDescription roughnessSamplerDesc;
                roughnessSamplerDesc.interpolation = SamplerInterpolation::Linear;
                roughnessSamplerDesc.wrapping = SamplerWrapping::Repeat;
                roughnessSamplerDesc.maxMip = m_images[normalTexture.value()].viewPerMip.size();
                roughnessSamplerDesc.useAnisotropy = true;
                roughnessSampler = createSampler(roughnessSamplerDesc);
            }
            const auto roughnessTextureResource = ImageResource(
                roughnessTexture.value(),
                0,
                7);

            const auto roughnessSamplerResource = SamplerResource(
                roughnessSampler.value(),
                3);

            resources.sampledImages.push_back(roughnessTextureResource);
            resources.samplers.push_back(roughnessSamplerResource);
        }
        MeshMaterial material;
        material.flags = pass.materialFeatures;
        material.descriptorSet = allocateDescriptorSet(pass.materialSetLayout);
        updateDescriptorSet(material.descriptorSet, resources);

        mesh.materials.push_back(material);
    }

    /*
    save and return handle
    */
    MeshHandle handle = m_meshes.size();
    m_meshes.push_back(mesh);

    return handle;
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
    case ImageFormat::RGBA8:            format = VK_FORMAT_R8G8B8A8_UNORM;          aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    case ImageFormat::RG16_sFloat:      format = VK_FORMAT_R16G16_SFLOAT;           aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    case ImageFormat::R11G11B10_uFloat: format = VK_FORMAT_B10G11R11_UFLOAT_PACK32; aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    case ImageFormat::RGBA32_sFloat:    format = VK_FORMAT_R32G32B32A32_SFLOAT;     aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT; break;
    case ImageFormat::Depth16:          format = VK_FORMAT_D16_UNORM;               aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT; break;
    case ImageFormat::Depth32:          format = VK_FORMAT_D32_SFLOAT;              aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT; break;
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
    case(MipCount::FullChain): mipCount = 1 + std::floor(std::log2(std::max(std::max(desc.width, desc.height), desc.depth))); break;
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
    imageInfo.pQueueFamilyIndices = &m_context.queueFamilies.graphicsQueueIndex;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto res = vkCreateImage(m_context.device, &imageInfo, nullptr, &image.vulkanHandle);
    assert(res == VK_SUCCESS);

    //allocate memory;
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(m_context.device, image.vulkanHandle, &memoryRequirements);
    image.memory = allocateMemory(memoryRequirements, 0);
    vkBindImageMemory(m_context.device, image.vulkanHandle, image.memory, 0);

    //create image view
    for (uint32_t i = 0; i < mipCount; i++) {
        image.viewPerMip.push_back(createImageView(image, viewType, i, mipCount - i, aspectFlag));
    }

    //fill with data
    if (desc.initialData.size() != 0) {
        transferDataIntoImage(image, desc.initialData.data(), desc.initialData.size());
    }

    /*
    generate mipmaps
    */
    if (desc.autoCreateMips) {
        generateMipChain(image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    ImageHandle handle;
    if (m_freeImageHandles.size() > 0) {
        handle = m_freeImageHandles.back();
        m_freeImageHandles.pop_back();
        m_images[handle] = image;
    }
    else {
        handle = m_images.size();
        m_images.push_back(image);
    }
    return handle;
}

/*
=========
createUniformBuffer
=========
*/
StorageBufferHandle RenderBackend::createUniformBuffer(const BufferDescription& description) {
    std::vector<uint32_t> queueFamilies = {
        m_context.queueFamilies.transferQueueFamilyIndex,
        m_context.queueFamilies.graphicsQueueIndex,
        m_context.queueFamilies.computeQueueIndex };

    Buffer uniformBuffer = createBufferInternal(description.size, queueFamilies,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (description.initialData != nullptr) {
        fillBuffer(uniformBuffer, description.initialData, description.size);
    }

    StorageBufferHandle handle = m_uniformBuffers.size();
    m_uniformBuffers.push_back(uniformBuffer);
    return handle;
}

/*
=========
createStorageBuffer
=========
*/
StorageBufferHandle RenderBackend::createStorageBuffer(const BufferDescription& description) {
    std::vector<uint32_t> queueFamilies = {
        m_context.queueFamilies.transferQueueFamilyIndex,
        m_context.queueFamilies.graphicsQueueIndex,
        m_context.queueFamilies.computeQueueIndex};

    Buffer storageBuffer = createBufferInternal(description.size, queueFamilies, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    StorageBufferHandle handle = m_storageBuffers.size();
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
    const auto res = vkCreateSampler(m_context.device, &samplerInfo, nullptr, &sampler);
    assert(res == VK_SUCCESS);

    SamplerHandle handle = m_samplers.size();
    m_samplers.push_back(sampler);
    return handle;
}

/*
=========
setSwapchainInputImage
=========
*/
void RenderBackend::setSwapchainInputImage(ImageHandle image) {
    m_swapchainInputImage = image;
}

/*
==================

private functions

==================
*/

void RenderBackend::prepareRenderPasses(const ImageHandle swapchainOutputImage) {

    /*
    update descriptor set
    */
    for (const auto pass : m_renderPassExecutions) {
        updateDescriptorSet(m_renderPasses[pass.handle].descriptorSet, pass.resources);
    }
    
    m_renderPassInternalExecutions.clear();
    auto renderPassesToAdd = m_renderPassExecutions;
    m_renderPassExecutions.clear();

    /*
    order passes
    iterate over passes, add them if possible
    adding is possible if all parents have already been added
    index is reset if pass is added to recheck condition for previous passes
    */
    for (int i = 0; i < renderPassesToAdd.size();) {
        const auto pass = renderPassesToAdd[i];
        bool parentsAvaible = true;
        for (const auto parent : pass.parents) {
            bool parentFound = false;
            for (uint32_t j = 0; j < m_renderPassInternalExecutions.size(); j++) {
                if (m_renderPassInternalExecutions[j].handle == parent) {
                    parentFound = true;
                    break;
                }
            }
            if (!parentFound) {
                parentsAvaible = false;
                i++;
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
            /*
            remove pass by swapping with end
            */
            renderPassesToAdd[i] = renderPassesToAdd.back();
            renderPassesToAdd.pop_back();
            i = 0;
        }
    }
    assert(renderPassesToAdd.size() == 0); //all passes must have been added

    /*
    update swapchain copy pass resource and add to executions
    */
    const auto swapchainOutput = ImageResource(swapchainOutputImage, 0, 0);

    const auto swapchainInput = ImageResource(m_swapchainInputImage, 0, 1);

    const auto swapchainImageExtent = m_images[swapchainOutputImage].extent;
    RenderPassExecutionInternal swapchainCopy;
    swapchainCopy.handle = m_swapchain.copyToSwapchainPass;
    swapchainCopy.dispatches[0] = std::ceilf(swapchainImageExtent.width / 8.f);
    swapchainCopy.dispatches[1] = std::ceilf(swapchainImageExtent.height / 8.f);
    swapchainCopy.dispatches[2] = 1;

    RenderPassResources swapchainCopyResources;
    swapchainCopyResources.storageImages = { swapchainInput, swapchainOutput };
    updateDescriptorSet(m_renderPasses[m_swapchain.copyToSwapchainPass].descriptorSet, swapchainCopyResources);
    m_renderPassInternalExecutions.push_back(swapchainCopy);

    /*
    add to external executions so barriers are set correctly
    */
    RenderPassExecution swapchainCopyExecution;
    swapchainCopyExecution.handle = m_swapchain.copyToSwapchainPass;
    swapchainCopyExecution.resources = swapchainCopyResources;
    m_renderPassExecutions.push_back(swapchainCopyExecution);

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
            Image& image = m_images[storageImage.image];

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
                const auto& layoutBarriers = createImageBarriers(image, requiredLayout, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, 0, image.layoutPerMip.size());
                barriers.insert(barriers.end(), layoutBarriers.begin(), layoutBarriers.end());
                image.currentlyWriting = true;
            }
        }

        /*
        sampled images
        */
        for (auto& sampledImage : resources.sampledImages) {
            Image& image = m_images[sampledImage.image];

            /*
            check if any mip levels need a layout transition
            */
            const VkImageLayout requiredLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            bool needsLayoutTransition = false;
            for (const auto& layout : image.layoutPerMip) {
                if (layout != requiredLayout) {
                    needsLayoutTransition = true;
                }
            }

            if (image.currentlyWriting | needsLayoutTransition) {
                const auto& layoutBarriers = createImageBarriers(image, requiredLayout, VK_ACCESS_SHADER_READ_BIT, 0, image.viewPerMip.size());
                barriers.insert(barriers.end(), layoutBarriers.begin(), layoutBarriers.end());
            }
        }

        /*
        attachments
        */
        const auto& pass = m_renderPasses[execution.handle];
        for (const auto imageHandle : pass.attachments) {
            Image& image = m_images[imageHandle];

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

                const auto& layoutBarriers = createImageBarriers(image, requiredLayout, access, 0, image.viewPerMip.size());
                barriers.insert(barriers.end(), layoutBarriers.begin(), layoutBarriers.end());
                image.currentlyWriting = true;
            }
        }

        m_renderPassInternalExecutions[i].imageBarriers = barriers;
    }

    /*
    add UI barriers
    */
    m_ui.barriers = createImageBarriers(m_images[swapchainOutputImage], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, 1);
}

/*
=========
submitRenderPass
=========
*/
void RenderBackend::submitRenderPass(const RenderPassExecutionInternal& execution) {
    barriersCommand(m_commandBuffer, execution.imageBarriers, execution.memoryBarriers);
    auto& pass = m_renderPasses[execution.handle];

    if (pass.isGraphicPass) {

        /*
        update pointer: might become invalid if pass vector was changed
        */
        pass.beginInfo.pClearValues = pass.clearValues.data();

        //prepare pass
        vkCmdBeginRenderPass(m_commandBuffer, &pass.beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipeline);
        vkCmdSetViewport(m_commandBuffer, 0, 1, &pass.viewport);
        vkCmdSetScissor(m_commandBuffer, 0, 1, &pass.scissor);

        for (const auto& mesh : pass.currentMeshRenderCommands) {

            /*
            vertex/index buffers
            */
            VkDeviceSize offset[] = { 0 };
            vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &mesh.vertexBuffer, offset);
            vkCmdBindIndexBuffer(m_commandBuffer, mesh.indexBuffer, offset[0], VK_INDEX_TYPE_UINT32);

            /*
            update push constants
            */
            glm::mat4 matrices[2] = { pass.viewProjectionMatrix * mesh.modelMatrix, mesh.modelMatrix };
            vkCmdPushConstants(m_commandBuffer, pass.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(matrices), &matrices);

            /*
            materials
            */
            VkDescriptorSet sets[3] = { m_globalDescriptorSet, pass.descriptorSet, mesh.materialSet };
            vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipelineLayout, 0, 3, sets, 0, nullptr);

            vkCmdDrawIndexed(m_commandBuffer, mesh.indexCount, 1, 0, 0, 0);
        }
        vkCmdEndRenderPass(m_commandBuffer);
    }
    else {
        vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipeline);
        VkDescriptorSet sets[3] = { m_globalDescriptorSet, pass.descriptorSet };
        vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pass.pipelineLayout, 0, 2, sets, 0, nullptr);
        vkCmdDispatch(m_commandBuffer, execution.dispatches[0], execution.dispatches[1], execution.dispatches[2]);
    }
}

/*
=========
waitForRenderFinished
=========
*/
void RenderBackend::waitForRenderFinished() {
    vkWaitForFences(m_context.device, 1, &m_imageInFlight, VK_TRUE, INT64_MAX);
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

    auto res = vkCreateInstance(&instanceInfo, nullptr, &m_context.vulkanInstance);
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
    return features.samplerAnisotropy && features.imageCubeArray;
}

/*
=========
pickPhysicalDevice
=========
*/
void RenderBackend::pickPhysicalDevice() {

    //enumerate devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_context.vulkanInstance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_context.vulkanInstance, &deviceCount, devices.data());

    //pick first suitable device
    for (const auto& device : devices) {
        QueueFamilies families;
        if (getQueueFamilies(device, &families) && hasRequiredDeviceFeatures(device)) {
            m_context.physicalDevice = device;
            break;
        }
    }

    if (m_context.physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find suitable physical device");
    }

    //retrieve and output device name
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_context.physicalDevice, &deviceProperties);
    
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

/*
=========
createLogicalDevice
=========
*/
void RenderBackend::createLogicalDevice() {

    //set removes duplicates
    std::set<uint32_t> uniqueQueueFamilies = {
        m_context.queueFamilies.graphicsQueueIndex,
        m_context.queueFamilies.computeQueueIndex,
        m_context.queueFamilies.presentationQueueIndex,
        m_context.queueFamilies.transferQueueFamilyIndex
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

    //device info
    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = nullptr;
    deviceInfo.flags = 0;
    deviceInfo.queueCreateInfoCount = queueInfos.size();
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.enabledLayerCount = 0;			//depreceated and ignored
    deviceInfo.ppEnabledLayerNames = nullptr;	//depreceated and ignored
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.pEnabledFeatures = &features;

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;

    auto res = vkCreateDevice(m_context.physicalDevice, &deviceInfo, nullptr, &m_context.device);
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
        (vkGetInstanceProcAddr(m_context.vulkanInstance, "vkCreateDebugReportCallbackEXT"));

    VkDebugReportCallbackEXT debugCallback;
    auto res = vkCreateDebugReportCallbackEXT(m_context.vulkanInstance, &callbackInfo, nullptr, &debugCallback);
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

    auto res = glfwCreateWindowSurface(m_context.vulkanInstance, window, nullptr, &m_swapchain.surface);
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
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.physicalDevice, m_swapchain.surface, &avaibleFormatCount, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("failed to query surface image format count");
    }
    std::vector<VkSurfaceFormatKHR> avaibleFormats(avaibleFormatCount);
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.physicalDevice, m_swapchain.surface, &avaibleFormatCount, avaibleFormats.data()) != VK_SUCCESS) {
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
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_context.physicalDevice, m_swapchain.surface, &surfaceCapabilities);

    swapchainInfo.imageExtent = surfaceCapabilities.currentExtent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    //set sharing mode depdening on if queues are the same
    uint32_t uniqueFamilies[2] = { m_context.queueFamilies.graphicsQueueIndex, m_context.queueFamilies.presentationQueueIndex };
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

    auto res = vkCreateSwapchainKHR(m_context.device, &swapchainInfo, nullptr, &m_swapchain.vulkanHandle);
    assert(res == VK_SUCCESS);
}

/*
=========
getSwapchainImages
=========
*/
void RenderBackend::getSwapchainImages(const uint32_t width, const uint32_t height) {

    uint32_t swapchainImageCount = 0;
    if (vkGetSwapchainImagesKHR(m_context.device, m_swapchain.vulkanHandle, &swapchainImageCount, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("failed to query swapchain image count");
    }
    std::vector<VkImage> swapchainImages;
    swapchainImages.resize(swapchainImageCount);
    if (vkGetSwapchainImagesKHR(m_context.device, m_swapchain.vulkanHandle, &swapchainImageCount, swapchainImages.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to query swapchain images");
    }

    m_swapchain.imageHandles.clear();
    for (const auto vulkanImage : swapchainImages) {
        //FIXME fill in rest of data
        Image image;
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
            m_images[handle] = image;
        }
        else {
            m_swapchain.imageHandles.push_back(m_images.size());
            m_images.push_back(image);
        }
    }
}

/*
=========
presentImage
=========
*/
void RenderBackend::presentImage(const uint32_t imageIndex, const VkSemaphore waitSemaphore) {

    VkPresentInfoKHR present = {};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pNext = nullptr;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &waitSemaphore;
    present.swapchainCount = 1;
    present.pSwapchains = &m_swapchain.vulkanHandle;
    present.pImageIndices = &imageIndex;

    VkResult presentResult = VK_SUCCESS;
    present.pResults = &presentResult;

    vkQueuePresentKHR(m_context.presentQueue, &present);

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

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_context.vulkanInstance;
    init_info.PhysicalDevice = m_context.physicalDevice;
    init_info.Device = m_context.device;
    init_info.QueueFamily = m_context.queueFamilies.graphicsQueueIndex;
    init_info.Queue = m_context.graphicQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_descriptorPool;
    init_info.Allocator = nullptr;
    init_info.MinImageCount = m_swapchain.minImageCount;
    init_info.ImageCount = m_swapchain.imageHandles.size();
    init_info.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&init_info, m_ui.renderPass);

    /*
    build fonts texture
    */
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    auto res = vkBeginCommandBuffer(m_commandBuffer, &begin_info);
    assert(res == VK_SUCCESS);

    ImGui_ImplVulkan_CreateFontsTexture(m_commandBuffer);

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &m_commandBuffer;
    res = vkEndCommandBuffer(m_commandBuffer);
    assert(res == VK_SUCCESS);
    res = vkQueueSubmit(m_context.graphicQueue, 1, &end_info, VK_NULL_HANDLE);
    assert(res == VK_SUCCESS);

    res = vkDeviceWaitIdle(m_context.device);
    assert(res == VK_SUCCESS);
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    /*
    create framebuffers
    */
    VkExtent2D extent;
    extent.width  = m_images[m_swapchain.imageHandles[0]].extent.width;
    extent.height = m_images[m_swapchain.imageHandles[0]].extent.height;
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
allocateMemory
=========
*/
VkDeviceMemory RenderBackend::allocateMemory(const VkMemoryRequirements& requirements, const VkMemoryPropertyFlags flags) {

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.allocationSize = requirements.size;

    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(m_context.physicalDevice, &memoryProperties);

    uint32_t memoryIndex = 0;
    bool foundMemory = false;

    //search for appropriate memory type
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((requirements.memoryTypeBits & (1 << i)) >> i) {
            if ((flags == 0) || (memoryProperties.memoryTypes[i].propertyFlags & flags)) {
                memoryIndex = i;
                foundMemory = true;
                break;
            }
        }
    }

    if (!foundMemory) {
        throw std::runtime_error("failed to find adequate memory type for allocation");
    }

    allocateInfo.memoryTypeIndex = memoryIndex;

    VkDeviceMemory memory;
    auto res = vkAllocateMemory(m_context.device, &allocateInfo, nullptr, &memory);
    assert(res == VK_SUCCESS);

    return memory;
}

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
    auto res = vkCreateImageView(m_context.device, &viewInfo, nullptr, &view);
    assert(res == VK_SUCCESS);

    return view;
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
    auto res = vkCreateBuffer(m_context.device, &bufferInfo, nullptr, &buffer.vulkanHandle);
    assert(res == VK_SUCCESS);

    //allocate memory
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(m_context.device, buffer.vulkanHandle, &memoryRequirements);

    buffer.memory = allocateMemory(memoryRequirements, memoryFlags);

    //attach memory to buffer
    res = vkBindBufferMemory(m_context.device, buffer.vulkanHandle, buffer.memory, 0);
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

    //create stagin buffer with image data
    Buffer stagingBuffer = createStagingBuffer(data, size);

    //begin command buffer for copying
    VkCommandBuffer copyBuffer = beginOneTimeUseCommandBuffer();

    /*
    layout transition
    */
    const auto toTransferDstBarrier = createImageBarriers(target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        VK_ACCESS_TRANSFER_READ_BIT, 0, target.viewPerMip.size());

    barriersCommand(copyBuffer, toTransferDstBarrier, std::vector<VkBufferMemoryBarrier> {});

    //copy command
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = target.extent.width;
    region.bufferImageHeight = target.extent.height; 
    region.imageSubresource = createSubresourceLayers(target, 0);
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = target.extent;

    vkCmdCopyBufferToImage(copyBuffer, stagingBuffer.vulkanHandle, target.vulkanHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vkEndCommandBuffer(copyBuffer);

    VkFence fence = submitOneTimeUseCmdBuffer(copyBuffer, m_context.transferQueue);

    vkWaitForFences(m_context.device, 1, &fence, VK_TRUE, UINT64_MAX);

    //cleanup
    vkDestroyFence(m_context.device, fence, nullptr);
    destroyBuffer(stagingBuffer);
    vkFreeCommandBuffers(m_context.device, m_transientCommandPool, 1, &copyBuffer);
}

void RenderBackend::generateMipChain(Image& image, const VkImageLayout oldLayout, const VkImageLayout newLayout) {

    /*
    check for linear filtering support
    */
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(m_context.physicalDevice, image.format, &formatProps);
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
    VkFence fence = submitOneTimeUseCmdBuffer(blitCmdBuffer, m_context.transferQueue);

    vkWaitForFences(m_context.device, 1, &fence, VK_TRUE, UINT64_MAX);

    //cleanup
    vkDestroyFence(m_context.device, fence, nullptr);
    vkFreeCommandBuffers(m_context.device, m_transientCommandPool, 1, &blitCmdBuffer);
}

/*
=========
fillBuffer
=========
*/
void RenderBackend::fillBuffer(Buffer target, const void* data, const VkDeviceSize size) {

    Buffer stagingBuffer = createStagingBuffer(data, size);

    //copy into target
    copyBuffer(stagingBuffer, target, size);

    vkDestroyBuffer(m_context.device, stagingBuffer.vulkanHandle, nullptr);
    vkFreeMemory(m_context.device, stagingBuffer.memory, nullptr);
}

/*
=========
createStagingBuffer
=========
*/
Buffer RenderBackend::createStagingBuffer(const void* data, const VkDeviceSize size) {
    //create staging buffer
    uint32_t memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    Buffer stagingBuffer = createBufferInternal(size, std::vector<uint32_t>{m_context.queueFamilies.transferQueueFamilyIndex}, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, memoryFlags);

    //fill staging buffer
    void* mappedData;
    vkMapMemory(m_context.device, stagingBuffer.memory, 0, size, 0, (void**)&mappedData);
    memcpy(mappedData, data, size);
    vkUnmapMemory(m_context.device, stagingBuffer.memory);

    return stagingBuffer;
}

/*
=========
copyBuffer
=========
*/
void RenderBackend::copyBuffer(const Buffer src, const Buffer dst, const VkDeviceSize size) {

    VkCommandBuffer copyCmdBuffer = beginOneTimeUseCommandBuffer();

    //copy command
    VkBufferCopy region = {};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = size;
    vkCmdCopyBuffer(copyCmdBuffer, src.vulkanHandle, dst.vulkanHandle, 1, &region);

    //end recording
    vkEndCommandBuffer(copyCmdBuffer);

    //submit
    VkFence fence = submitOneTimeUseCmdBuffer(copyCmdBuffer, m_context.transferQueue);

    vkWaitForFences(m_context.device, 1, &fence, VK_TRUE, UINT64_MAX);

    //cleanup
    vkDestroyFence(m_context.device, fence, nullptr);
    vkFreeCommandBuffers(m_context.device, m_transientCommandPool, 1, &copyCmdBuffer);
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
    auto res = vkCreateCommandPool(m_context.device, &poolInfo, nullptr, &pool);
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
    auto res = vkAllocateCommandBuffers(m_context.device, &bufferInfo, &commandBuffer);
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
    vkAllocateCommandBuffers(m_context.device, &command, &cmdBuffer);

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
    vkResetFences(m_context.device, 1, &fence);

    vkQueueSubmit(queue, 1, &submit, fence);

    return fence;
}

/*
==================

descriptors and layouts

==================
*/

/*
=========
createDescriptorPool
=========
*/
VkDescriptorPool RenderBackend::createDescriptorPool(const uint32_t maxSets) {

    VkDescriptorPoolCreateInfo poolInfo;
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = 0;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = 1;

    VkDescriptorPoolSize poolSize;
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = maxSets;

    poolInfo.pPoolSizes = &poolSize;

    VkDescriptorPool pool;
    auto res = vkCreateDescriptorPool(m_context.device, &poolInfo, nullptr, &pool);
    assert(res == VK_SUCCESS);

    return pool;
}

/*
=========
allocateDescriptorSet
=========
*/
VkDescriptorSet RenderBackend::allocateDescriptorSet(const VkDescriptorSetLayout setLayout) {

    VkDescriptorSetAllocateInfo setInfo;
    setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setInfo.pNext = nullptr;
    setInfo.descriptorPool = m_descriptorPool;
    setInfo.descriptorSetCount = 1;
    setInfo.pSetLayouts = &setLayout;

    VkDescriptorSet descriptorSet;
    vkAllocateDescriptorSets(m_context.device, &setInfo, &descriptorSet);

    return descriptorSet;
}

/*
=========
createDescriptorSet
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
        samplerInfo.sampler = m_samplers[resource.sampler];
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
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_images[resource.image].viewPerMip[resource.mipLevel];
        imageInfos[imageInfoIndex] = imageInfo;
        const auto writeSet = createWriteDescriptorSet(resource.binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, nullptr, &imageInfos[imageInfoIndex]);
        imageInfoIndex++;
        descriptorInfos.push_back(writeSet);
    }

    /*
    storage images
    */
    for (const auto& resource : resources.storageImages) {
        VkDescriptorImageInfo imageInfo;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = m_images[resource.image].viewPerMip[resource.mipLevel];
        imageInfos[imageInfoIndex] = imageInfo;
        const auto writeSet = createWriteDescriptorSet(resource.binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, nullptr, &imageInfos[imageInfoIndex]);
        imageInfoIndex++;
        descriptorInfos.push_back(writeSet);
    }

    /*
    uniform buffer
    */
    for (const auto& resource : resources.uniformBuffers) {
        Buffer buffer = m_uniformBuffers[resource.buffer];
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
        Buffer buffer = m_storageBuffers[resource.buffer];
        VkDescriptorBufferInfo bufferInfo;
        bufferInfo.buffer = buffer.vulkanHandle;
        bufferInfo.offset = 0;
        bufferInfo.range = buffer.size;
        bufferInfos[bufferInfoIndex] = bufferInfo;
        const auto writeSet = createWriteDescriptorSet(resource.binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &bufferInfos[bufferInfoIndex], nullptr);
        bufferInfoIndex++;
        descriptorInfos.push_back(writeSet);
    }

    vkUpdateDescriptorSets(m_context.device, descriptorInfos.size(), descriptorInfos.data(), 0, nullptr);
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
    auto res = vkCreateDescriptorSetLayout(m_context.device, &layoutInfo, nullptr, &setLayout);
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
    auto res = vkCreatePipelineLayout(m_context.device, &layoutInfo, nullptr, &layout);
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
RenderPass RenderBackend::createComputePassInternal(const ComputePassDescription& desc) {

    RenderPass pass;
    pass.computePassDesc = desc;
    VkComputePipelineCreateInfo pipelineInfo;
    VkPipelineShaderStageCreateInfo stageInfo;

    const auto spirV = loadShader(desc.shaderPath);

    VkShaderModule module = createShaderModule(spirV);
    ShaderReflection reflection = performComputeShaderReflection(spirV);
    pass.isGraphicPass = false;
    pass.descriptorSetLayout = createDescriptorSetLayout(reflection.shaderLayout);
    pass.pipelineLayout = createPipelineLayout(pass.descriptorSetLayout, VK_NULL_HANDLE, false);

    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.flags = 0;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = module;
    stageInfo.pName = "main";
    stageInfo.pSpecializationInfo = nullptr;

    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.flags = 0;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = pass.pipelineLayout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = 0;

    auto res = vkCreateComputePipelines(m_context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pass.pipeline);
    assert(res == VK_SUCCESS);

    //shader module no needed anymore
    vkDestroyShaderModule(m_context.device, module, nullptr);

    /*
    descriptor set
    */
    pass.descriptorSet = allocateDescriptorSet(pass.descriptorSetLayout);

    return pass;
}

/*
=========
createGraphicPassInternal
=========
*/
RenderPass RenderBackend::createGraphicPassInternal(const GraphicPassDescription& desc) {

    RenderPass pass;
    pass.graphicPassDesc = desc;
    pass.isGraphicPass = true;
    pass.attachmentDescriptions = desc.attachments;
    for (const auto attachment : desc.attachments) {
        pass.attachments.push_back(attachment.image);
    }

    /*
    load shader modules
     */

    GraphicShaderCode spirVCode;
    spirVCode.vertexCode   = loadShader(desc.shaderPaths.vertex);
    spirVCode.fragmentCode = loadShader(desc.shaderPaths.fragment);

    VkShaderModule vertexModule   = createShaderModule(spirVCode.vertexCode);
    VkShaderModule fragmentModule = createShaderModule(spirVCode.fragmentCode);

    VkShaderModule geometryModule = VK_NULL_HANDLE;
    VkShaderModule tesselationControlModule = VK_NULL_HANDLE;
    VkShaderModule tesselationEvaluationModule = VK_NULL_HANDLE;
    if (desc.shaderPaths.geometry.has_value()) {
        spirVCode.geometryCode = loadShader(desc.shaderPaths.geometry.value());
        geometryModule = createShaderModule(spirVCode.geometryCode.value());
    }
    if (desc.shaderPaths.tesselationControl.has_value()) {
        assert(desc.shaderPaths.tesselationEvaluation.has_value());   //both shaders must be defined or none

        spirVCode.tesselationControlCode    = loadShader(desc.shaderPaths.tesselationControl.value());
        spirVCode.tesselationEvaluationCode = loadShader(desc.shaderPaths.tesselationEvaluation.value());

        tesselationControlModule    = createShaderModule(spirVCode.tesselationControlCode.value());
        tesselationEvaluationModule = createShaderModule(spirVCode.tesselationEvaluationCode.value());
    }

    /*
    create module infos
    */
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    stages.push_back(createPipelineShaderStageInfos(vertexModule, VK_SHADER_STAGE_VERTEX_BIT));
    stages.push_back(createPipelineShaderStageInfos(fragmentModule, VK_SHADER_STAGE_FRAGMENT_BIT));

    if (geometryModule != VK_NULL_HANDLE) {
        stages.push_back(createPipelineShaderStageInfos(geometryModule, VK_SHADER_STAGE_GEOMETRY_BIT));
    }
    if (tesselationControlModule != VK_NULL_HANDLE) {
        stages.push_back(createPipelineShaderStageInfos(tesselationControlModule, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT));
        stages.push_back(createPipelineShaderStageInfos(tesselationEvaluationModule, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
    }

    /*
    shader reflection
    */
    ShaderReflection reflection = performShaderReflection(spirVCode);
    pass.vertexInputFlags = reflection.vertexInputFlags;
    pass.materialFeatures = reflection.materialFeatures;
    pass.descriptorSetLayout = createDescriptorSetLayout(reflection.shaderLayout);

    /*
    material set layout
    */
    ShaderLayout shaderLayout;
    if (reflection.materialFeatures & MATERIAL_FEATURE_FLAG_ALBEDO_TEXTURE) {
        shaderLayout.samplerBindings.push_back(0);
        shaderLayout.sampledImageBindings.push_back(4);
    }
    if (reflection.materialFeatures & MATERIAL_FEATURE_FLAG_NORMAL_TEXTURE) {
        shaderLayout.samplerBindings.push_back(1);
        shaderLayout.sampledImageBindings.push_back(5);
    }
    if (reflection.materialFeatures & MATERIAL_FEATURE_FLAG_METALIC_TEXTURE) {
        shaderLayout.samplerBindings.push_back(2);
        shaderLayout.sampledImageBindings.push_back(6);
    }
    if (reflection.materialFeatures & MATERIAL_FEATURE_FLAG_ROUGHNESS_TEXTURE) {
        shaderLayout.samplerBindings.push_back(3);
        shaderLayout.sampledImageBindings.push_back(7);
    }
    pass.materialSetLayout = createDescriptorSetLayout(shaderLayout);
    pass.pipelineLayout = createPipelineLayout(pass.descriptorSetLayout, pass.materialSetLayout, true);

    /*
    get width and height from output
    */
    assert(desc.attachments.size() >= 1); //need at least a single attachment to write to
    const uint32_t width = m_images[desc.attachments[0].image].extent.width;
    const uint32_t height = m_images[desc.attachments[0].image].extent.height;

    /*
    validate attachments
    */
    for (const auto attachmentDefinition : desc.attachments) {

        const Image attachment = m_images[attachmentDefinition.image];

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
        const auto image = m_images[attachment.image];
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
    const auto inputAssemblyState = createDefaultInputAssemblyInfo();
    const auto tesselationState = desc.shaderPaths.tesselationControl.has_value() ? &createTesselationState(desc.patchControlPoints) : nullptr;

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

    vkCreateGraphicsPipelines(m_context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pass.pipeline);

    //shader modules aren't needed anymore
    vkDestroyShaderModule(m_context.device, vertexModule, nullptr);
    vkDestroyShaderModule(m_context.device, fragmentModule, nullptr);
    if (geometryModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_context.device, geometryModule, nullptr);
    }
    if (tesselationControlModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_context.device, tesselationControlModule, nullptr);
        vkDestroyShaderModule(m_context.device, tesselationEvaluationModule, nullptr);
    }

    /*
    clear values
    */
    for (const auto& attachment : desc.attachments) {
        const auto image = m_images[attachment.image];
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
    pass.descriptorSet = allocateDescriptorSet(pass.descriptorSetLayout);

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

        const auto image = m_images[attachment.image];
        VkImageLayout layout = isDepthFormat(image.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        desc.flags = 0;
        desc.format = m_images[attachment.image].format;
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

    auto res = vkCreateRenderPass(m_context.device, &info, nullptr, &pass);
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
        const auto image = m_images[attachment.image];
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
    auto res = vkCreateFramebuffer(m_context.device, &framebufferInfo, nullptr, &framebuffer);
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
    auto res = vkCreateShaderModule(m_context.device, &moduleInfo, nullptr, &shaderModule);
    assert(res == VK_SUCCESS);

    return shaderModule;
}

/*
createPipelineShaderStageInfos
*/
VkPipelineShaderStageCreateInfo RenderBackend::createPipelineShaderStageInfos(const VkShaderModule module, const VkShaderStageFlagBits stage) {
    VkPipelineShaderStageCreateInfo info;
    info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.pNext                  = nullptr;
    info.flags                  = 0;
    info.stage                  = stage;
    info.module                 = module;
    info.pName                  = "main";
    info.pSpecializationInfo    = nullptr;
    return info;
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
    rasterization.depthClampEnable = VK_FALSE;
    rasterization.rasterizerDiscardEnable = VK_FALSE;
    rasterization.polygonMode = polygonMode;
    rasterization.cullMode = cullFlags;
    rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
    auto res = vkCreateSemaphore(m_context.device, &semaphoreInfo, nullptr, &semaphore);
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
    auto res = vkCreateFence(m_context.device, &fenceInfo, nullptr, &fence);
    assert(res == VK_SUCCESS);

    return fence;
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

    const auto image = m_images[handle];
    for (const auto& view : image.viewPerMip) {
        vkDestroyImageView(m_context.device, view, nullptr);
    }
    
    /*
    swapchain images have no manualy allocated memory
    they are deleted by the swapchain
    view has to be destroyed manually though
    */
    if (image.memory == VK_NULL_HANDLE) {
        return;
    }
    vkFreeMemory(m_context.device, image.memory, nullptr);
    vkDestroyImage(m_context.device, image.vulkanHandle, nullptr);
}

/*
=========
destroyBuffer
=========
*/
void RenderBackend::destroyBuffer(const Buffer& buffer) {
    vkDestroyBuffer(m_context.device, buffer.vulkanHandle, nullptr);
    vkFreeMemory(m_context.device, buffer.memory, nullptr);
}

/*
=========
destroyMesh
=========
*/
void RenderBackend::destroyMesh(const Mesh& mesh) {
    for (const auto& buffer : mesh.vertexBuffers) {
        destroyBuffer(buffer.vertexBuffer);
    }
    destroyBuffer(mesh.indexBuffer);
}

/*
=========
destroyRenderPass
=========
*/
void RenderBackend::destroyRenderPass(const RenderPass& pass) {
    if (pass.isGraphicPass) {
        vkDestroyRenderPass(m_context.device, pass.vulkanRenderPass, nullptr);
        vkDestroyFramebuffer(m_context.device, pass.beginInfo.framebuffer, nullptr);
        vkDestroyDescriptorSetLayout(m_context.device, pass.materialSetLayout, nullptr);
    }
    vkDestroyPipelineLayout(m_context.device, pass.pipelineLayout, nullptr);
    vkDestroyPipeline(m_context.device, pass.pipeline, nullptr);
    vkDestroyDescriptorSetLayout(m_context.device, pass.descriptorSetLayout, nullptr);
}