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

struct GLFWwindow;

struct Swapchain {
    VkSurfaceKHR                surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR          surfaceFormat = {};
    VkSwapchainKHR              vulkanHandle;
    uint32_t                    minImageCount;
    std::vector<ImageHandle>    imageHandles;
    VkSemaphore                 imageAvaible;
};

struct UIRenderInfo {
    std::vector<VkRenderPassBeginInfo>  passBeginInfos;
    std::vector<VkFramebuffer>          framebuffers;
    VkRenderPass                        renderPass = VK_NULL_HANDLE;
};

//structs that are referenced by VkPipelineShaderStageCreateInfo
struct VulkanShaderCreateAdditionalStructs {
    VkSpecializationInfo                    specialisationInfo = {};
    std::vector<VkSpecializationMapEntry>   specilisationMap;
    std::vector<char>                       specialisationData;
};

//because they are extensions they need to be acquired using vkGetDeviceProcAddr
struct VulkanDebugUtilsFunctions {
    PFN_vkCmdBeginDebugUtilsLabelEXT    vkCmdBeginDebugUtilsLabelEXT    = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT      vkCmdEndDebugUtilsLabelEXT      = nullptr;
    PFN_vkCmdInsertDebugUtilsLabelEXT   vkCmdInsertDebugUtilsLabelEXT   = nullptr;
    PFN_vkCreateDebugUtilsMessengerEXT  vkCreateDebugUtilsMessengerEXT  = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT = nullptr;
    PFN_vkQueueBeginDebugUtilsLabelEXT  vkQueueBeginDebugUtilsLabelEXT  = nullptr;
    PFN_vkQueueEndDebugUtilsLabelEXT    vkQueueEndDebugUtilsLabelEXT    = nullptr;
    PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT = nullptr;
    PFN_vkSetDebugUtilsObjectNameEXT    vkSetDebugUtilsObjectNameEXT    = nullptr;
    PFN_vkSetDebugUtilsObjectTagEXT     vkSetDebugUtilsObjectTagEXT     = nullptr;
    PFN_vkSubmitDebugUtilsMessageEXT    vkSubmitDebugUtilsMessageEXT    = nullptr;
};

struct DescriptorPoolAllocationSizes {
    uint32_t setCount       = 0;
    uint32_t imageSampled   = 0;
    uint32_t imageStorage   = 0;
    uint32_t uniformBuffer  = 0;
    uint32_t storageBuffer  = 0;
    uint32_t sampler        = 0;
};

struct DescriptorPool {
    VkDescriptorPool vkPool = VK_NULL_HANDLE;
    DescriptorPoolAllocationSizes freeAllocations;
};

struct TimestampQuery {
    uint32_t startQuery = 0;
    uint32_t endQuery = 0;
    std::string name;
};

//per pass execution time
struct RenderPassTime{
    float timeMs = 0; //time in milliseconds
    std::string name;
};

struct VulkanRasterizationStateCreateInfo {
    VkPipelineRasterizationStateCreateInfo	baseInfo;
    VkPipelineRasterizationConservativeStateCreateInfoEXT conservativeInfo;
};

struct RenderPassExecutionEntry {
    RenderPassType type;
    int index;
};

struct UniformBufferFillOrder {
    UniformBufferHandle buffer;
    std::vector<char> data;
};

struct StorageBufferFillOrder {
    StorageBufferHandle buffer;
    std::vector<char> data;
};

class RenderBackend {
public:

    void setup(GLFWwindow* window);
    void shutdown();
    void recreateSwapchain(const uint32_t width, const uint32_t height, GLFWwindow* window);

    //checks if any shaders are out of date, if so reloads them and reconstructs the corresponding pipeline
    void updateShaderCode();

    //multiple images must be resizable at once, as framebuffers may only be updated once all images have been resized    
    void resizeImages(const std::vector<ImageHandle>& images, const uint32_t width, const uint32_t height);

    //old frame cleanup and new frame preparations, must be called before render pass executions can be set
    void newFrame();

    //must be called after newFrame and before startDrawcallRecording
    void setGraphicPassExecution(const GraphicPassExecution& execution);

    //must be called after newFrame and before startDrawcallRecording
    void setComputePassExecution(const ComputePassExecution& execution);

    //prepares for mesh drawcalls, must be called after all render pass executions have been set
    void prepareForDrawcallRecording();

    //must be called after startDrawcallRecording
    void drawMeshes(const std::vector<MeshHandle> meshHandles, const char* pushConstantData, const RenderPassHandle passHandle, const int workerIndex);

    //actual copy is deferred to before submitting, to avoid stalling cpu until previous frame is finished rendering
    //results in an extra copy of data
    void setUniformBufferData(const UniformBufferHandle buffer, const void* data, const size_t size);

    //actual copy is deferred to before submitting, to avoid stalling cpu until previous frame is finished rendering
    //results in an extra copy of data
    void setStorageBufferData(const StorageBufferHandle buffer, const void* data, const size_t size);

    //must be set once before creating renderpasses
    void setGlobalDescriptorSetLayout(const ShaderLayout& layout);

    void setGlobalDescriptorSetResources(const RenderPassResources& resources);

    //set path and specialisation constants, forces recompile and pipeline recreation
    void updateGraphicPassShaderDescription(const RenderPassHandle passHandle, const GraphicPassShaderDescriptions& desc);
    void updateComputePassShaderDescription(const RenderPassHandle passHandle, const ShaderDescription& desc);

    //actual rendering of frame using commands generated from drawMesh calls
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

    ImageDescription getImageDescription(const ImageHandle image);
    void waitForGpuIdle();  // only use for rare events, such as resizing on settings change

private:

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

    std::vector<RenderPassExecutionEntry> m_renderPassExecutions;
    std::vector<GraphicPassExecution> m_graphicPassExecutions;
    std::vector<ComputePassExecution> m_computePassExecutions;

    void submitGraphicPass(const GraphicPassExecution& execution,
        const RenderPassBarriers& barriers, const VkCommandBuffer commandBuffer, const VkFramebuffer framebuffer);

    void submitComputePass(const ComputePassExecution& execution,
        const RenderPassBarriers& barriers, const VkCommandBuffer commandBuffer);

    void waitForRenderFinished();

    //framebuffer stuff
    std::vector<VkFramebuffer> m_transientFramebuffers[2];

    std::vector<VkFramebuffer> createGraphicPassFramebuffer(const std::vector<GraphicPassExecution>& execution);
    VkFramebuffer createVulkanFramebuffer(const std::vector<RenderTarget>& targets, const VkRenderPass renderpass);
    void destroyFramebuffer(const VkFramebuffer& framebuffer);

    bool validateAttachments(const std::vector<RenderTarget>& attachments);
    bool imageHasAttachmentUsageFlag(const Image& image);
    glm::uvec2 getResolutionFromRenderTargets(const std::vector<RenderTarget>& targets);

    /*
    =========
    context
    =========
    */
    VkDebugReportCallbackEXT    m_debugCallback = VK_NULL_HANDLE;

#ifdef USE_VK_VALIDATION_LAYERS
    const bool m_useValidationLayers = true;
#else
    const bool m_useValidationLayers = false;
#endif

    std::vector<const char*>    getRequiredInstanceExtensions();
    void                        createVulkanInstance();
    bool                        hasRequiredDeviceFeatures(const VkPhysicalDevice physicalDevice);
    void                        pickPhysicalDevice();
    void                        createLogicalDevice();
    VkDebugReportCallbackEXT    setupDebugCallbacks();

    //returns true if all family indices have been found, in that case indices are writen to QueueFamilies pointer
    bool getQueueFamilies(const VkPhysicalDevice device, QueueFamilies* pOutQueueFamilies);

    //debug marker use an extension and as such need to get function pointers
    void acquireDebugUtilsExtFunctionsPointers();

    VulkanDebugUtilsFunctions m_debugExtFunctions;

    /*
    =========
    swapchain
    =========
    */
    Swapchain m_swapchain;


    void createSurface(GLFWwindow* window);
    void chooseSurfaceFormat();
    void createSwapChain();

    void getSwapchainImages(const uint32_t width, const uint32_t height);
    void presentImage(const VkSemaphore waitSemaphore);

    /*
    =========
    imgui
    =========
    */
    UIRenderInfo m_ui;
    void setupImgui(GLFWwindow* window);

    //currently scheduled renderpass
    std::vector<RenderPassExecution>    m_framePasses;
    uint32_t                            m_swapchainInputImageIndex = 0;
    ImageHandle                         m_swapchainInputImageHandle;

    /*
    =========
    resources
    =========
    */
    VkMemoryAllocator m_vkAllocator;

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

    void mapOverRenderpassTempImages(std::function<void(const int renderpassImage, const int tempImageIndex)> function);
    void allocateTemporaryImages();
    void resetAllocatedTempImages();
    void updateRenderPassDescriptorSets();
    void startGraphicPassRecording(); // after this drawcalls can be submitted, but no renderpass executions issued anymore

    std::vector<TemporaryImage> m_temporaryImages;

    struct AllocatedTempImage {
        bool usedThisFrame = false;
        Image image;
    };

    std::vector<AllocatedTempImage> m_allocatedTempImages;  //allocated images are shared by non-overlapping temporary images

    std::vector<UniformBufferFillOrder> m_deferredUniformBufferFills;
    std::vector<StorageBufferFillOrder> m_deferredStorageBufferFills;

    //freed indices
    std::vector<ImageHandle>    m_freeImageHandles;

    //staging buffer
    VkDeviceSize m_stagingBufferSize = 1048576; //1mb
    Buffer m_stagingBuffer;

    Image createImageInternal(const ImageDescription& description, const void* initialData, const size_t initialDataSize);
    VkImageView createImageView(const Image& image, const VkImageViewType viewType, const uint32_t baseMip, const uint32_t mipLevels, const VkImageAspectFlags aspectMask);
    Buffer      createBufferInternal(const VkDeviceSize size, const std::vector<uint32_t>& queueFamilies, const VkBufferUsageFlags usage, const uint32_t memoryFlags);

    VkImageSubresourceLayers createSubresourceLayers(const Image& image, const uint32_t mipLevel);

    //copies data into a temporary staging buffer, then transfers data into image
    void transferDataIntoImage(Image& target, const void* data, const VkDeviceSize size);
    void generateMipChain(Image& image, const VkImageLayout newLayout);

    //fills buffer using the staging buffer
    void fillBuffer(Buffer target, const void* data, const VkDeviceSize size);
    void fillHostVisibleCoherentBuffer(Buffer target, const void* data, const VkDeviceSize size);

    /*
    =========
    commands
    =========
    */

    std::vector<VkCommandPool> m_drawcallCommandPools;

    VkCommandPool   m_commandPool = VK_NULL_HANDLE;
    VkCommandPool   m_transientCommandPool = VK_NULL_HANDLE; //used for short lived copy command buffer and such

    //primary command buffers used for all rendering
    //two so one can be filled while the other is still rendering
    VkCommandBuffer m_commandBuffers[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };

    uint32_t m_frameIndex = 0;
    uint32_t m_frameIndexMod2 = 0;


    VkCommandPool   createCommandPool(const uint32_t queueFamilyIndex, const VkCommandPoolCreateFlagBits flags);
    VkCommandBuffer allocateCommandBuffer(const VkCommandBufferLevel level, const VkCommandPool& pool);

    //allocate and begin a one time use command buffer
    //returned command buffer must be manually destroyed	
    VkCommandBuffer beginOneTimeUseCommandBuffer();

    //Queue recording must have been ended before
    //returned fence must be manually destroyed
    VkFence submitOneTimeUseCmdBuffer(VkCommandBuffer cmdBuffer, VkQueue queue);

    void startDebugLabel(const VkCommandBuffer cmdBuffer, const std::string& name);
    void endDebugLabel(const VkCommandBuffer cmdBuffer);

    /*
    =========
    descriptors and layouts
    =========
    */
    VkDescriptorSet m_globalDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_globalDescriptorSetLayout = VK_NULL_HANDLE;    //contains global info, always bound to set 0

    //discriptor pools are added as existing ones run out
    std::vector<DescriptorPool> m_descriptorPools;

    //the imgui pool is just passed to the library, no need for allocation counting
    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;

    //imgui requires a single sizeable pool, so it's handled separately
    void createImguiDescriptorPool();

    //create pool using m_descriptorPoolInitialAllocationSizes and add to m_descriptorPools
    void createDescriptorPool();

    //how many types of descriptors are allocated from a new pool
    const DescriptorPoolAllocationSizes m_descriptorPoolInitialAllocationSizes = {
        128, //set count
        128, //imageSampled
        128, //imageStorage
        128, //uniformBuffer
        128, //storageBuffer
        128  //sampler
    };

    DescriptorPoolAllocationSizes descriptorSetAllocationSizeFromShaderLayout(const ShaderLayout& layout);

    //creates new descriptor pool if needed
    //currently now way to free descriptor set
    VkDescriptorSet         allocateDescriptorSet(const VkDescriptorSetLayout setLayout, const DescriptorPoolAllocationSizes& requiredSizes);
    void                    updateDescriptorSet(const VkDescriptorSet set, const RenderPassResources& resources);
    VkDescriptorSetLayout   createDescriptorSetLayout(const ShaderLayout& shaderLayout);

    //isGraphicsPass controls if the push constant range is setup for the MVP matrix    
    VkPipelineLayout        createPipelineLayout(const VkDescriptorSetLayout setLayout, const size_t pushConstantSize,
        const VkShaderStageFlags stageFlags);


    /*
    =========
    renderpass creation
    =========
    */

    //actual creation of internal objects
    //split from public function to allow use when reloading shader	
    ComputePass createComputePassInternal(const ComputePassDescription& desc, const std::vector<uint32_t>& spirV);
    GraphicPass createGraphicPassInternal(const GraphicPassDescription& desc, const GraphicPassShaderSpirV& spirV);

    VkRenderPass    createVulkanRenderPass(const std::vector<Attachment>& attachments);
    VkShaderModule  createShaderModule(const std::vector<uint32_t>& code);

    //outAdditionalInfo has to be from parent scope to keep pointers to info structs valid
    VkPipelineShaderStageCreateInfo         createPipelineShaderStageInfos(const VkShaderModule module, const VkShaderStageFlagBits stage,
        const std::vector<SpecialisationConstant>& specialisationInfo, VulkanShaderCreateAdditionalStructs* outAdditionalInfo);
    VkPipelineInputAssemblyStateCreateInfo  createDefaultInputAssemblyInfo();
    VkPipelineTessellationStateCreateInfo   createTesselationState(const uint32_t patchControlPoints);

    VulkanRasterizationStateCreateInfo		createRasterizationState(const RasterizationConfig& raster);
    VkPipelineMultisampleStateCreateInfo    createDefaultMultisamplingInfo();
    VkPipelineDepthStencilStateCreateInfo   createDepthStencilState(const DepthTest& depthTest);

    //renderpass creation utilities    
    bool isDepthFormat(VkFormat format);
    bool isDepthFormat(ImageFormat format);

    //renderpass barriers    
    void barriersCommand(const VkCommandBuffer commandBuffer, const std::vector<VkImageMemoryBarrier>& imageBarriers, const std::vector<VkBufferMemoryBarrier>& memoryBarriers);

    //create necessary image barriers
    //multiple barriers may be needed, as mip levels may have differing layouts
    //sets image.layout to newLayout, image.currentAccess to dstAccess and image.currentlyWriting to false
    std::vector<VkImageMemoryBarrier> createImageBarriers(Image& image, const VkImageLayout newLayout,
        const VkAccessFlags dstAccess, const uint32_t baseMip, const uint32_t mipLevels);

    VkBufferMemoryBarrier   createBufferBarrier(const Buffer& buffer, const VkAccessFlagBits srcAccess, const VkAccessFlagBits dstAccess);

    /*
    =========
    sync objects
    =========
    */
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence     m_renderFinishedFence = VK_NULL_HANDLE;


    VkSemaphore createSemaphore();
    VkFence     createFence();


    //timestamp queries
    const uint32_t m_timestampQueryPoolQueryCount = 100;

    //two pools to allow reset and usage of one while the other is rendered by gpu
    VkQueryPool m_timestampQueryPools[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };

    float m_nanosecondsPerTimestamp = 1.f;

    //one count for each frame in flight
    uint32_t m_timestampQueryCounts[2] = { 0, 0 };

    std::vector<TimestampQuery> m_timestampQueriesPerFrame[2];	//one query list per frame in flight
    std::vector<RenderPassTime> m_renderpassTimings;

    //must be called in an active command buffer
    //issues vulkan timestamp query and increments timestamp query counter
    //returns query index
    uint32_t issueTimestampQuery(const VkCommandBuffer cmdBuffer, const int poolIndex);

    //does not handle queryType VK_QUERY_TYPE_PIPELINE_STATISTICS 
    VkQueryPool createQueryPool(const VkQueryType queryType, const uint32_t queryCount);
    void resetTimestampQueryPool(const int poolIndex);

    float m_timeOfLastGPUSubmit = 0.f;
    float m_lastFrameCPUTime = 0.f;

    void checkVulkanResult(const VkResult result);

    /*
    =========
    resource destruction
    =========
    */
    void destroyImage(const ImageHandle handle);
    void destroyImageInternal(const Image& image);
    void destroyBuffer(const Buffer& buffer);
    void destroyMesh(const Mesh& mesh);
    void destroyGraphicPass(const GraphicPass& pass);
    void destroyComputePass(const ComputePass& pass);
};

extern RenderBackend gRenderBackend;