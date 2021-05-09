#pragma once
#include "pch.h"

#include <vulkan/vulkan.h>
#include "VertexInput.h"
#include "Runtime/Rendering/ResourceDescriptions.h"
#include "VulkanAllocation.h"

struct ShaderLayout {
    std::vector<uint32_t> samplerBindings;
    std::vector<uint32_t> sampledImageBindings;
    std::vector<uint32_t> storageImageBindings;
    std::vector<uint32_t> uniformBufferBindings;
    std::vector<uint32_t> storageBufferBindings;
};

struct Image {

    VkImage                     vulkanHandle = VK_NULL_HANDLE;
    std::vector<VkImageView>    viewPerMip;
    VkFormat                    format = VK_FORMAT_MAX_ENUM;

    std::vector<VkImageLayout>  layoutPerMip;
    VkAccessFlags               currentAccess = 0;
    bool                        currentlyWriting = false;

    //description backup in case of resize
    ImageDescription desc;

    bool isSwapchainImage = false;
    VulkanAllocation memory;
    int32_t globalDescriptorSetIndex = -1;
};

struct Buffer {
    VkBuffer vulkanHandle = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VulkanAllocation memory;
    bool isBeingWritten = false;
};

//disable warning caused by vulkan use
#pragma warning( push )
#pragma warning( disable : 26812) //use of unscoped enum

struct Mesh {
    uint32_t        indexCount = 0;
    Buffer          indexBuffer;
    VkIndexType     indexPrecision = VK_INDEX_TYPE_NONE_KHR;
    Buffer          vertexBuffer;
};

//reenable warning
#pragma warning( pop )

struct GraphicPassShaderSpirV {
    std::vector<uint32_t> vertex;
    std::vector<uint32_t> fragment;
    std::optional<std::vector<uint32_t>> geometry;
    std::optional<std::vector<uint32_t>> tessEval; // tesselation evaluation
    std::optional<std::vector<uint32_t>> tessCtrl; // tesselation control
};

struct GraphicPassShaderGLSL {
    std::vector<char> vertex;
    std::vector<char> fragment;
    std::optional<std::vector<char>> geometry;
    std::optional<std::vector<char>> tessEval; // tesselation evaluation
    std::optional<std::vector<char>> tessCtrl; // tesselation control
};

struct GraphicPass {
    //used to reconstruct pass when reloading shader
    GraphicPassDescription graphicPassDesc;

    GraphicShadersHandle shaderHandle;

    VkRenderPass            vulkanRenderPass    = VK_NULL_HANDLE;
    VkPipeline              pipeline            = VK_NULL_HANDLE;
    VkPipelineLayout        pipelineLayout      = VK_NULL_HANDLE;
    //two sets to allow updating on while other is used for rendering
    VkDescriptorSet         descriptorSets[2]	= { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkDescriptorSetLayout   descriptorSetLayout = VK_NULL_HANDLE;

    VertexInputFlags vertexInputFlags = VertexInputFlags(0);

    //frameCount * workerCount command buffers
    //every frame can be recorded while the other is executed
    //and commands can be recorded in parallel on every worker thread
    std::vector<VkCommandBuffer> meshCommandBuffers;

    std::vector<VkClearValue> clearValues;

    //shader path cache to speed up out of date check    
    std::string shaderCachePathAbsolute;
    std::string shaderSrcPathAbsolute;

    size_t pushConstantSize = 0;
};

struct ComputePass {
    ComputePassDescription computePassDesc;

    ComputeShaderHandle shaderHandle;

    VkRenderPass            vulkanRenderPass    = VK_NULL_HANDLE;
    VkPipeline              pipeline            = VK_NULL_HANDLE;
    VkPipelineLayout        pipelineLayout      = VK_NULL_HANDLE;
    //two sets to allow updating on while other is used for rendering
    VkDescriptorSet         descriptorSets[2]	= { VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkDescriptorSetLayout   descriptorSetLayout = VK_NULL_HANDLE;
    VkRenderPassBeginInfo   beginInfo = {};

    /*
    shader path cache to speed up out of date check
    */
    std::string shaderCachePathAbsolute;
    std::string shaderSrcPathAbsolute;

    size_t pushConstantSize;
};

struct RenderPassBarriers {
    std::vector<VkBufferMemoryBarrier>  memoryBarriers;
    std::vector<VkImageMemoryBarrier>   imageBarriers;
};