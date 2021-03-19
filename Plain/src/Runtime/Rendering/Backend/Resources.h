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
    VkExtent3D                  extent = {};
    ImageType                   type = ImageType::Type2D;

    /*
    current status
    */
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

//can be updated, limited feature set
struct DynamicMesh {
    uint32_t    indexCount = 0;
    Buffer      vertexBuffer;
    Buffer      indexBuffer;
};

struct GraphicPassShaderSpirV {
    std::vector<uint32_t> vertex;
    std::vector<uint32_t> fragment;
    std::optional<std::vector<uint32_t>> geometry;
    std::optional<std::vector<uint32_t>> tessellationEvaluation;
    std::optional<std::vector<uint32_t>> tessellationControl;
};

struct GraphicPassShaderGLSL {
    std::vector<char> vertex;
    std::vector<char> fragment;
    std::optional<std::vector<char>> geometry;
    std::optional<std::vector<char>> tesselationEvaluation;
    std::optional<std::vector<char>> tesselationControl;
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

    VertexInputFlags                vertexInputFlags = VertexInputFlags(0);

	//two to allow recording while gpu is working
    VkCommandBuffer meshCommandBuffers[2];

    std::vector<VkClearValue> clearValues;

    //shader path cache to speed up out of date check    
    std::string shaderCachePathAbsolute;
    std::string shaderSrcPathAbsolute;

	size_t pushConstantSize;
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

struct Framebuffer {
    FramebufferDescription desc; //stored for recreation after image resize
    VkFramebuffer vkHandle;
};