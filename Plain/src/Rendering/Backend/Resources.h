#pragma once
#include "pch.h"

#include <vulkan/vulkan.h>
#include "VertexInput.h"
#include "Rendering/ResourceDescriptions.h"
#include "VulkanAllocation.h"

/*
shader bindings
*/
struct ShaderLayout {
    std::vector<uint32_t> samplerBindings;
    std::vector<uint32_t> sampledImageBindings;
    std::vector<uint32_t> storageImageBindings;
    std::vector<uint32_t> uniformBufferBindings;
    std::vector<uint32_t> storageBufferBindings;
};

/*
image
*/
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

    /*
    description backup in case of resize
    */
    ImageDescription desc;

    bool isSwapchainImage = false;
    VulkanAllocation memory;
};

/*
buffer
*/
struct Buffer {
    VkBuffer vulkanHandle = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VulkanAllocation memory;
};

/*
the same mesh can be used with different passes
passes may need different vertex attributes and therefore different vertex buffers
the same is true for material features
*/

enum class MaterialFeatureFlags {
    AlbedoTexture   = 0x00000001,
    NormalTexture   = 0x00000002,
    SpecularTexture = 0x00000004,
};

MaterialFeatureFlags operator&(const MaterialFeatureFlags l, const MaterialFeatureFlags r);
MaterialFeatureFlags operator|(const MaterialFeatureFlags l, const MaterialFeatureFlags r);


struct MeshVertexBuffer {
    Buffer              buffer;
    VertexInputFlags    flags = VertexInputFlags(0);
};

struct MeshMaterial {
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    MaterialFeatureFlags flags = MaterialFeatureFlags(0);
};

struct Mesh {
    uint32_t                        indexCount = 0;
    Buffer                          indexBuffer;
    VkIndexType                     indexPrecision = VK_INDEX_TYPE_NONE_KHR;
    std::vector<MeshVertexBuffer>   vertexBuffers;  //one vertex buffer per render pass with unique vertex input
    std::vector<MeshMaterial>       materials;      //one material per render pass with unique material feature set
};

//can be updated, limited feature set
struct DynamicMesh {
    uint32_t            indexCount = 0;
    MeshVertexBuffer    vertexBuffer;
    Buffer              indexBuffer;
};

//primary and secondary matrix are supplied to shader via push constants
//primary is MPV mostly
//secondary is pass dependant
//for example model matrix for shading pass and previous MVP for motion vectors in depth prepass
struct MeshRenderCommand {
    VkBuffer        indexBuffer;
    VkBuffer        vertexBuffer;
    uint32_t        indexCount;
    VkIndexType     indexPrecision;
    VkDescriptorSet materialSet;
    glm::mat4       primaryMatrix;
    glm::mat4       secondaryMatrix;
};

//lacks materials
struct DynamicMeshRenderCommand {
    VkBuffer        indexBuffer;
    VkBuffer        vertexBuffer;
    uint32_t        indexCount;
    glm::mat4       primaryMatrix;
    glm::mat4       secondaryMatrix;
};

struct GraphicPassShaderSpirV {
    std::vector<uint32_t> vertex;
    std::vector<uint32_t> fragment;
    std::optional<std::vector<uint32_t>> geometry;
    std::optional<std::vector<uint32_t>> tesselationEvaluation;
    std::optional<std::vector<uint32_t>> tesselationControl;
};

/*
renderpass
*/
struct GraphicPass {
    //used to reconstruct pass when reloading shader
    GraphicPassDescription graphicPassDesc;
    
    std::filesystem::file_time_type lastModifiedShader;

    VkRenderPass            vulkanRenderPass    = VK_NULL_HANDLE;
    VkPipeline              pipeline            = VK_NULL_HANDLE;
    VkPipelineLayout        pipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSet         descriptorSet       = VK_NULL_HANDLE;
    VkDescriptorSetLayout   descriptorSetLayout = VK_NULL_HANDLE;
    VkRenderPassBeginInfo   beginInfo = {};

    VkViewport                      viewport;
    VkRect2D                        scissor;
    VertexInputFlags                vertexInputFlags = VertexInputFlags(0);

    std::vector<MeshRenderCommand>          meshRenderCommands;
    std::vector<DynamicMeshRenderCommand>   dynamicMeshRenderCommands;

    std::vector<VkClearValue>       clearValues;
    std::vector<ImageHandle>        attachments;

    /*
    stored in case of a resize
    */
    std::vector<Attachment>         attachmentDescriptions;

    /*
    some passes may no use all material features
    therefore every pass has it's own materialSetLayout and a bitset of features
    */
    VkDescriptorSetLayout   materialSetLayout = VK_NULL_HANDLE;
    MaterialFeatureFlags    materialFeatures;

    /*
    shader path cache to speed up out of date check
    */
    std::string shaderCachePathAbsolute;
    std::string shaderSrcPathAbsolute;
};

struct ComputePass {
    ComputePassDescription computePassDesc;

    std::filesystem::file_time_type lastModifiedShader;

    VkRenderPass            vulkanRenderPass    = VK_NULL_HANDLE;
    VkPipeline              pipeline            = VK_NULL_HANDLE;
    VkPipelineLayout        pipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSet         descriptorSet       = VK_NULL_HANDLE;
    VkDescriptorSetLayout   descriptorSetLayout = VK_NULL_HANDLE;
    VkRenderPassBeginInfo   beginInfo = {};

    /*
    shader path cache to speed up out of date check
    */
    std::string shaderCachePathAbsolute;
    std::string shaderSrcPathAbsolute;
};

/*
stores all information needed to execute a pass
the external version is used to order, update and create barriers
*/
struct RenderPassExecutionInternal {
    RenderPassHandle                    handle;
    uint32_t                            dispatches[3] = {1, 1, 1}; //for compute only
    std::vector<VkBufferMemoryBarrier>  memoryBarriers;
    std::vector<VkImageMemoryBarrier>   imageBarriers;
};