#pragma once
#include "pch.h"

#include <vulkan/vulkan.h>
#include "VertexInput.h"

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
    VkDeviceMemory              memory = VK_NULL_HANDLE;
    std::vector<VkImageView>    viewPerMip;
    VkFormat                    format;
    VkExtent3D                  extent;
    ImageType                   type;

    /*
    current status
    */
    std::vector<VkImageLayout>  layoutPerMip;
    VkAccessFlags               currentAccess;
    bool                        currentlyWriting = false;

    /*
    description backup in case of resize
    */
    ImageDescription desc;
};

/*
buffer
*/
struct Buffer {
    VkBuffer        vulkanHandle;
    VkDeviceMemory  memory;
    VkDeviceSize    size;
};

/*
the same mesh can be used with different passes
passes may need different vertex attributes and therefore different vertex buffers
the same is true for material features
*/

typedef enum MaterialFeatureFlags {
    MATERIAL_FEATURE_FLAG_ALBEDO_TEXTURE = 0x00000001,
    MATERIAL_FEATURE_FLAG_NORMAL_TEXTURE = 0x00000002,
    MATERIAL_FEATURE_FLAG_METALIC_TEXTURE = 0x00000004,
    MATERIAL_FEATURE_FLAG_ROUGHNESS_TEXTURE = 0x00000008
} MaterialFeatureFlags;

struct MeshVertexBuffer {
    Buffer              vertexBuffer;
    VertexInputFlags    flags;
};

struct MeshMaterial {
    VkDescriptorSet         descriptorSet;
    MaterialFeatureFlags    flags;
};

struct Mesh {
    uint32_t                        indexCount = 0;
    Buffer                          indexBuffer;
    std::vector<MeshVertexBuffer>   vertexBuffers;
    std::vector<MeshMaterial>      materials;
};

struct MeshRenderCommand {
    VkBuffer        indexBuffer;
    VkBuffer        vertexBuffer;
    uint32_t        indexCount;
    VkDescriptorSet materialSet;
    glm::mat4       modelMatrix;
};

/*
renderpass
*/
struct RenderPass {
    //used to reconstruct pass when reloading shader
    std::optional<GraphicPassDescription> graphicPassDesc;
    std::optional<ComputePassDescription> computePassDesc;

    bool                                isGraphicPass; //else compute
    VkRenderPass                        vulkanRenderPass = VK_NULL_HANDLE;
    VkPipeline                          pipeline = VK_NULL_HANDLE;
    VkPipelineLayout                    pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSet                     descriptorSet;
    VkDescriptorSetLayout               descriptorSetLayout;
    VkRenderPassBeginInfo               beginInfo;

    //only used in graphic passes
    VkViewport                      viewport;
    VkRect2D                        scissor;
    glm::mat4                       viewProjectionMatrix = glm::mat4(1.f);
    VertexInputFlags                vertexInputFlags = VertexInputFlags(0);
    std::vector<MeshRenderCommand>  currentMeshRenderCommands;
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
    VkDescriptorSetLayout   materialSetLayout;
    MaterialFeatureFlags    materialFeatures;
};

/*
stores all information needed to execute a pass
the external version is used to order, update and create barriers
*/
struct RenderPassExecutionInternal {
    RenderPassHandle                    handle;
    uint32_t                            dispatches[3]; //for compute only
    std::vector<VkBufferMemoryBarrier>  memoryBarriers;
    std::vector<VkImageMemoryBarrier>   imageBarriers;
};