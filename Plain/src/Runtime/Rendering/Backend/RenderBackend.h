#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

#include "Runtime/Rendering/RenderHandles.h"
#include "Runtime/Rendering/ResourceDescriptions.h"
#include "Resources.h"
#include "VulkanContext.h"
#include "VkMemoryAllocator.h"
#include "Runtime/Rendering/Backend/SpirvReflection.h"
#include "Common/MeshData.h"
#include "ShaderFileManager.h"
#include "RenderPassManager.h"
#include "VulkanDescriptorPool.h"
#include "RenderPassExecution.h"
#include "PerFrameResources.h"
#include "ImGuiIntegration.h"
#include "VulkanSwapchain.h"
#include "VulkanTransfer.h"

struct GLFWwindow;

struct UniformBufferFillOrder {
    UniformBufferHandle buffer;
    std::vector<char>   data;
};

struct StorageBufferFillOrder {
    StorageBufferHandle buffer;
    std::vector<char>   data;
};

class RenderBackend {
public:

    void setup(GLFWwindow* window);
    void shutdown();
    void recreateSwapchain(const uint32_t width, const uint32_t height, GLFWwindow* window);

    // TODO: this should not be available to to the frontend, remove all usages and this function
    void waitForGPUIdle();

    // checks if any shaders are out of date, if so reloads them and reconstructs the corresponding pipeline
    void updateShaderCode();

    // multiple images must be resizable at once, as framebuffers may only be updated once all images have been resized    
    void resizeImages(const std::vector<ImageHandle>& images, const uint32_t width, const uint32_t height);

    // old frame cleanup and new frame preparations, must be called before render pass executions can be set
    void newFrame();

    // must be called after newFrame and before startDrawcallRecording
    void setGraphicPassExecution(const GraphicPassExecution& execution);

    // must be called after newFrame and before startDrawcallRecording
    void setComputePassExecution(const ComputePassExecution& execution);

    // prepares for mesh drawcalls, must be called after all render pass executions have been set
    void prepareForDrawcallRecording();

    // must be called after startDrawcallRecording
    void drawMeshes(const std::vector<MeshHandle> meshHandles, const char* pushConstantData, const RenderPassHandle passHandle, const int workerIndex);

    // actual copy is deferred to before submitting, to avoid stalling cpu until previous frame is finished rendering
    // results in an extra copy of data
    void setUniformBufferData(const UniformBufferHandle buffer, const void* data, const size_t size);

    // actual copy is deferred to before submitting, to avoid stalling cpu until previous frame is finished rendering
    // results in an extra copy of data
    void setStorageBufferData(const StorageBufferHandle buffer, const void* data, const size_t size);

    // must be set once before creating renderpasses
    void setGlobalDescriptorSetLayout(const ShaderLayout& layout);

    void setGlobalDescriptorSetResources(const RenderPassResources& resources);

    // set path and specialisation constants, forces recompile and pipeline recreation
    void updateGraphicPassShaderDescription(const RenderPassHandle passHandle, const GraphicPassShaderDescriptions& desc);
    void updateComputePassShaderDescription(const RenderPassHandle passHandle, const ShaderDescription& desc);

    // actual rendering of frame using commands generated from drawMesh calls
    void renderFrame(const bool presentToScreen);

    uint32_t getImageGlobalTextureArrayIndex(const ImageHandle image);

    //the public create pass functions save the descriptions and create the handle, then call 
    //the internal ones for creation of actual API objects    
    RenderPassHandle createComputePass(const ComputePassDescription& desc);
    RenderPassHandle createGraphicPass(const GraphicPassDescription& desc);

    std::vector<MeshHandle> createMeshes(const std::vector<MeshBinary>& meshes);

    ImageHandle         createImage(const ImageDescription& description, const void* initialData, const size_t initialDataSize);
    UniformBufferHandle createUniformBuffer(const UniformBufferDescription& desc);
    StorageBufferHandle createStorageBuffer(const StorageBufferDescription& desc);
    SamplerHandle       createSampler(const SamplerDescription& description);

    // temporary images are the preferred way to create render targets and other temp images
    // they are valid for one frame
    // their lifetime is automatically managed and existing images are reused where possible
    ImageHandle createTemporaryImage(const ImageDescription& description);

    ImageHandle getSwapchainInputImage();

    void getMemoryStats(uint64_t* outAllocatedSize, uint64_t* outUsedSize) const;

    std::vector<RenderPassTime> getRenderpassTimings() const;
    float getLastFrameCPUTime() const;

    ImageDescription getImageDescription(const ImageHandle handle);

private:

    void reloadComputePass(const ComputePassShaderReloadInfo& reloadInfo);
    void reloadGraphicPass(const GraphicPassShaderReloadInfo& reloadInfo);

    Buffer createStagingBuffer();

    ShaderFileManager m_shaderFileManager;

    VkDescriptorSetLayout m_globalTextureArrayDescriporSetLayout = VK_NULL_HANDLE;
    void initGlobalTextureArrayDescriptorSetLayout();

    VkDescriptorSet m_globalTextureArrayDescriptorSet = VK_NULL_HANDLE;
    void initGlobalTextureArrayDescriptorSet();

    VkDescriptorPool m_globalTextureArrayDescriptorPool = VK_NULL_HANDLE;

    void setGlobalTextureArrayDescriptorSetTexture(const VkImageView imageView, const uint32_t index);

    size_t m_globalTextureArrayDescriptorSetTextureCount = 0;
    std::vector<int32_t> m_globalTextureArrayDescriptorSetFreeTextureIndices;

    std::vector<RenderPassBarriers> createRenderPassBarriers();

    std::vector<RenderPassExecutionEntry>   m_renderPassExecutions;
    std::vector<GraphicPassExecution>       m_graphicPassExecutions;
    std::vector<ComputePassExecution>       m_computePassExecutions;

    void submitRenderPasses(PerFrameResources *inOutFrameResources, const std::vector<RenderPassBarriers> barriers);

    void submitGraphicPass(
        const GraphicPassExecution& execution,
        const RenderPassBarriers&   barriers, 
        PerFrameResources*          inOutFrameResources, 
        const VkFramebuffer         framebuffer);

    void submitComputePass(
        const ComputePassExecution& execution,
        const RenderPassBarriers&   barriers, 
        PerFrameResources*          inOutFrameResources);

    void waitForRenderFinished();
    void executeDeferredBufferFillOrders();

    std::vector<VkFramebuffer>  createGraphicPassFramebuffers(const std::vector<GraphicPassExecution>& execution);
    std::vector<VkImageView>    getImageViewsFromRenderTargets(const std::vector<RenderTarget>& targets);

    bool        validateRenderTargets(const std::vector<RenderTarget>& attachments);
    glm::uvec2  getResolutionFromRenderTargets(const std::vector<RenderTarget>& targets);

    Swapchain m_swapchain;

    ImGuiRenderResources m_imguiResources;

    // currently scheduled renderpass
    std::vector<RenderPassExecution>    m_framePasses;
    ImageHandle                         m_swapchainInputImageHandle;

    VkMemoryAllocator m_vkAllocator;

    RenderPassManager       m_renderPasses;
    std::vector<Image>      m_images;
    std::vector<Mesh>       m_meshes;
    std::vector<VkSampler>  m_samplers;
    std::vector<Buffer>     m_uniformBuffers;
    std::vector<Buffer>     m_storageBuffers;

    Image& getImageRef(const ImageHandle handle);

    struct TemporaryImage {
        ImageDescription desc;
        int allocationIndex = -1;
    };

    //void mapOverRenderpassTempImages(std::function<void(const int renderpassImage, const int tempImageIndex)> function);
    void allocateTemporaryImages();
    void resetAllocatedTempImages();
    void updateRenderPassDescriptorSets();

    // after this drawcalls can be submitted, but no renderpass executions issued anymore
    void startGraphicPassRecording(const GraphicPassExecution& execution, const VkFramebuffer framebuffer); 

    std::vector<TemporaryImage> m_temporaryImages;

    struct AllocatedTempImage {
        bool    usedThisFrame = false;
        Image   image;
    };

    // allocated images are shared by non-overlapping temporary images
    std::vector<AllocatedTempImage> m_allocatedTempImages;

    std::vector<UniformBufferFillOrder> m_deferredUniformBufferFills;
    std::vector<StorageBufferFillOrder> m_deferredStorageBufferFills;

    std::vector<ImageHandle> m_freeImageHandles;

    TransferResources m_transferResources;

    Image   createImageInternal(const ImageDescription& description, const Data& initialData);
    Buffer  createBufferInternal(const VkDeviceSize size, const std::vector<uint32_t>& queueFamilies, const VkBufferUsageFlags usage, const uint32_t memoryFlags);

    void addImageToGlobalDescriptorSetLayout(Image& image);

    // pools for secondary command buffers per thread 
    std::vector<VkCommandPool>  m_drawcallCommandPools;
    VkCommandPool               m_commandPool = VK_NULL_HANDLE;

    VkDescriptorSet         m_globalDescriptorSet       = VK_NULL_HANDLE;

    // contains global info, always bound to set 0
    VkDescriptorSetLayout   m_globalDescriptorSetLayout = VK_NULL_HANDLE;

    // descriptor pools are added as existing ones run out
    std::vector<DescriptorPool> m_descriptorPools;

    // creates new descriptor pool if needed
    // currently no way to free descriptor set
    VkDescriptorPool    findFittingDescriptorPool(const DescriptorPoolAllocationSizes& requiredSizes);
    void                updateDescriptorSet(const VkDescriptorSet set, const RenderPassResources& resources);

    // actual creation of internal objects
    // split from public function to allow usage when reloading shader	
    ComputePass createComputePassInternal(const ComputePassDescription& desc, const std::vector<uint32_t>& spirV);
    GraphicPass createGraphicPassInternal(const GraphicPassDescription& desc, const GraphicPassShaderSpirV& spirV);

    VkSemaphore m_renderFinishedSemaphore   = VK_NULL_HANDLE;
    VkFence     m_renderFinishedFence       = VK_NULL_HANDLE;

    std::vector<RenderPassTime> m_renderpassTimings;
    PerFrameResources           m_perFrameResources[2];

    float m_timeOfLastGPUSubmit = 0.f;
    float m_lastFrameCPUTime    = 0.f;

    void destroyImage(const ImageHandle handle);
    void destroyImageInternal(const Image& image);
    void destroyBuffer(const Buffer& buffer);
    void destroyMesh(const Mesh& mesh);
};

extern RenderBackend gRenderBackend;