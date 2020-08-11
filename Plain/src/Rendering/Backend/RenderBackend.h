#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

#include "Rendering/RenderHandles.h"
#include "Rendering/ResourceDescriptions.h"
#include "Resources.h"
#include "Rendering/MeshDataInternal.h"
#include "VulkanContext.h"
#include "VkMemoryAllocator.h"
#include "Rendering/Backend/SpirvReflection.h"

struct GLFWwindow;

struct Swapchain {
    VkSurfaceKHR                    surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR              surfaceFormat = {};
    VkSwapchainKHR                  vulkanHandle;
    uint32_t                        minImageCount;
    std::vector<ImageHandle>        imageHandles;
    RenderPassHandle                copyToSwapchainPass;
    VkSemaphore                     imageAvaible;
};

struct UIRenderInfo {
    std::vector<VkImageMemoryBarrier>   barriers;
    std::vector<VkRenderPassBeginInfo>  passBeginInfos;
    std::vector<VkFramebuffer>          framebuffers;
    VkRenderPass                        renderPass;
};

//structs that are reference by VkPipelineShaderStageCreateInfo
struct VulkanShaderCreateAdditionalStructs {
    VkSpecializationInfo                    specialisationInfo;
    std::vector<VkSpecializationMapEntry>   specilisationMap;
};

//because they are extensions they need to be acquired using vkGetDeviceProcAddr
struct VulkanDebugUtilsFunctions {
    PFN_vkCmdBeginDebugUtilsLabelEXT    vkCmdBeginDebugUtilsLabelEXT;
    PFN_vkCmdEndDebugUtilsLabelEXT      vkCmdEndDebugUtilsLabelEXT;
    PFN_vkCmdInsertDebugUtilsLabelEXT   vkCmdInsertDebugUtilsLabelEXT;
    PFN_vkCreateDebugUtilsMessengerEXT  vkCreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
    PFN_vkQueueBeginDebugUtilsLabelEXT  vkQueueBeginDebugUtilsLabelEXT;
    PFN_vkQueueEndDebugUtilsLabelEXT    vkQueueEndDebugUtilsLabelEXT;
    PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT;
    PFN_vkSetDebugUtilsObjectNameEXT    vkSetDebugUtilsObjectNameEXT;
    PFN_vkSetDebugUtilsObjectTagEXT     vkSetDebugUtilsObjectTagEXT;
    PFN_vkSubmitDebugUtilsMessageEXT    vkSubmitDebugUtilsMessageEXT;
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
    VkDescriptorPool vkPool;
    DescriptorPoolAllocationSizes freeAllocations;
};

/*
RenderPass handles are shared between compute and graphics to allow easy dependency management in the frontend
the first bit of the handle indicates wether a handle is a compute or graphic pass
this class wraps the vectors and indexing to avoid having to explicitly call a translation function for every access
e.g. passes(handleToIndex(handle))
*/
class RenderPasses {
public:

    bool isGraphicPassHandle(const RenderPassHandle handle);

    /*
    add
    */
    RenderPassHandle addGraphicPass(const GraphicPass pass);
    RenderPassHandle addComputePass(const ComputePass pass);

    /*
    getters
    */
    uint32_t getNGraphicPasses();
    uint32_t getNComputePasses();

    GraphicPass& getGraphicPassRefByHandle(const RenderPassHandle handle);
    ComputePass& getComputePassRefByHandle(const RenderPassHandle handle);

    GraphicPass& getGraphicPassRefByIndex(const uint32_t index);
    ComputePass& getComputePassRefByIndex(const uint32_t index);

    /*
    updates
    */
    void updateGraphicPassByHandle(const GraphicPass pass, const RenderPassHandle handle);
    void updateComputePassByHandle(const ComputePass pass, const RenderPassHandle handle);

    void updateGraphicPassByIndex(const GraphicPass pass, const uint32_t index);
    void updateComputePassByIndex(const ComputePass pass, const uint32_t index);

private:
    std::vector<GraphicPass> m_graphicPasses;
    std::vector<ComputePass> m_computePasses;

    /*
    utilities
    renderpass handles are indices into the respective vector
    graphic passes have the first bit set to 1
    */

    uint32_t graphicPassHandleToIndex(const RenderPassHandle handle);
    uint32_t computePassHandleToIndex(const RenderPassHandle handle);
    RenderPassHandle indexToGraphicPassHandle(const uint32_t index);
    RenderPassHandle indexToComputePassHandle(const uint32_t index);
};

class RenderBackend {
public:

	void setup(GLFWwindow* window);
	void teardown();
    void recreateSwapchain(const uint32_t width, const uint32_t height, GLFWwindow* window);

    //checks if any shaders are out of date, if so reloads them and reconstructs the corresponding pipeline
    void updateShaderCode();

    /*
    multiple images must be resizable at once, as framebuffers may only be updated once all images have been resized
    */
    void resizeImages(const std::vector<ImageHandle>& images, const uint32_t width, const uint32_t height);

    void newFrame();
    void setRenderPassExecution(const RenderPassExecution& execution);

    /*
    actual draw command is deferred until renderFrame is called
    meshHandle must be obtained from createMesh function
    */
    void drawMeshes(const std::vector<MeshHandle> meshHandles, const std::vector<glm::mat4>& modelMatrices, const std::vector<RenderPassHandle>& passes);
    void drawDynamicMeshes(const std::vector<DynamicMeshHandle> handles, const std::vector<glm::mat4>& modelMatrices, const std::vector<RenderPassHandle>& passes);


    void setViewProjectionMatrix(const glm::mat4& viewProjection, const RenderPassHandle pass);
    void setGlobalShaderInfo(const GlobalShaderInfo& info);

    //set path and specialisation constants, forces recompile and pipeline recreation
    void updateGraphicPassShaderDescription(const RenderPassHandle passHandle, const GraphicPassShaderDescriptions& desc);
    void updateComputePassShaderDescription(const RenderPassHandle passHandle, const ShaderDescription& desc);

    /*
    actual rendering of frame using commands generated from drawMesh calls
    commands are reset after frame rendering so backend is ready to record next frame
    */
    void renderFrame();    

    /*
    the public create pass functions save the descriptions and create the handle, then call 
    the internal ones for creation of actual API objects
    */
    RenderPassHandle    createComputePass(const ComputePassDescription& desc);
    RenderPassHandle    createGraphicPass(const GraphicPassDescription& desc);

    std::vector<MeshHandle> createMeshes(const std::vector<MeshDataInternal>& meshes, 
        const std::vector<RenderPassHandle>& passes);

    //dynamic meshes can be updated
    //they use host visible memory which makes the update fast but rendering slow
    //only positions are supported
    //intended to use for debug visualisation which are updated per frame
    std::vector<DynamicMeshHandle> createDynamicMeshes(const std::vector<uint32_t>& maxPositionsPerMesh,
        const std::vector<uint32_t>& maxIndicesPerMesh);
    void updateDynamicMeshes(const std::vector<DynamicMeshHandle>& handles, 
        const std::vector<std::vector<glm::vec3>>& positionsPerMesh,
        const std::vector<std::vector<uint32_t>>&  indicesPerMesh);

    ImageHandle             createImage(const ImageDescription& description);
    UniformBufferHandle     createUniformBuffer(const BufferDescription& description);
    StorageBufferHandle     createStorageBuffer(const BufferDescription& description);
    SamplerHandle           createSampler(const SamplerDescription& description);

    void setSwapchainInputImage(ImageHandle image);

    void getMemoryStats(uint32_t* outAllocatedSize, uint32_t* outUsedSize);

private:

    /*
    calculates pass order, updates descritor sets, creates barriers
    */
    void prepareRenderPasses(const ImageHandle swapchainOutputImage);

    std::vector<RenderPassExecution>            m_renderPassExecutions;
    std::vector<RenderPassExecutionInternal>    m_renderPassInternalExecutions;

    /*
    submits render commands of pass
    m_commandBuffer must be recording
    */
    void submitRenderPass(const RenderPassExecutionInternal& execution, const VkCommandBuffer commandBuffer);

    void waitForRenderFinished();

    /*
    =========
    context
    =========
    */
    VkDebugReportCallbackEXT    m_debugCallback = VK_NULL_HANDLE;

    /*
    validation layers are disabled in release build using macro
    */
#ifdef NDEBUG
    const bool m_useValidationLayers = true;
#else
    const bool m_useValidationLayers = true;
#endif

    std::vector<const char*>    getRequiredExtensions();
    void                        createVulkanInstance();
    bool                        hasRequiredDeviceFeatures(const VkPhysicalDevice physicalDevice);
    void                        pickPhysicalDevice();
    void                        createLogicalDevice();
    VkDebugReportCallbackEXT    setupDebugCallbacks();

    /*
    returns true if all family indices have been found, in that case indices are writen to QueueFamilies pointer
    */
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
    void presentImage(const uint32_t imageIndex, const VkSemaphore waitSemaphore);

    /*
    =========
    imgui
    =========
    */
    UIRenderInfo m_ui;
    void setupImgui(GLFWwindow* window);

    /*
    currently scheduled renderpass
    */
    std::vector<RenderPassExecution>    m_framePasses;
    ImageHandle                         m_swapchainInputImage;

    /*
    =========
    resources
    =========
    */
    VkMemoryAllocator m_vkAllocator;

    RenderPasses            m_renderPasses;
    std::vector<Image>      m_images;
    std::vector<Mesh>       m_meshes;
    std::vector<DynamicMesh>m_dynamicMeshes;
    std::vector<VkSampler>  m_samplers;
    std::vector<Buffer>     m_uniformBuffers;
    std::vector<Buffer>     m_storageBuffers;

    /*
    freed indices
    */
    std::vector<ImageHandle>    m_freeImageHandles;

    UniformBufferHandle m_globalShaderInfoBuffer;

    //staging buffer
    VkDeviceSize m_stagingBufferSize = 1048576; //1mb
    Buffer m_stagingBuffer;

    VkImageView createImageView(const Image image, const VkImageViewType viewType, const uint32_t baseMip, const uint32_t mipLevels, const VkImageAspectFlags aspectMask);
    Buffer      createBufferInternal(const VkDeviceSize size, const std::vector<uint32_t>& queueFamilies, const VkBufferUsageFlags usage, const uint32_t memoryFlags);
    MeshHandle  createMeshInternal(const MeshDataInternal data, const std::vector<RenderPassHandle>& passes);
    DynamicMeshHandle  createDynamicMeshInternal(const uint32_t maxPositions, const uint32_t maxIndices);

    VkImageSubresourceLayers createSubresourceLayers(const Image& image, const uint32_t mipLevel);

    /*
    copies data into a temporary staging buffer, then transfers data into image
    */
    void transferDataIntoImage(Image& target, const void* data, const VkDeviceSize size);
    void generateMipChain(Image& image, const VkImageLayout newLayout);

    /*
    fills buffer using the staging buffer
    */
    void fillBuffer(Buffer target, const void* data, const VkDeviceSize size);
    void fillHostVisibleCoherentBuffer(Buffer target, const void* data, const VkDeviceSize size);

    /*
    =========
    commands 
    =========
    */
    VkCommandPool   m_commandPool;
    VkCommandPool   m_transientCommandPool; //used for short lived copy command buffer and such
    /*
    primary command buffers used for all rendering
    two so one can be filled while the other is still rendering
    */
    VkCommandBuffer m_commandBuffers[2];

    uint32_t m_currentCommandBufferIndex = 0;

    
    VkCommandPool   createCommandPool(const uint32_t queueFamilyIndex, const VkCommandPoolCreateFlagBits flags);

    //allocates a single primary command buffer from m_commandPool
    VkCommandBuffer allocateCommandBuffer();

    /*
    allocate and begin a one time use command buffer
    returned command buffer must be manually destroyed
    */
    VkCommandBuffer beginOneTimeUseCommandBuffer();

    /*
    Queue recording must have been ended before
    returned fence must be manually destroyed
    */
    VkFence submitOneTimeUseCmdBuffer(VkCommandBuffer cmdBuffer, VkQueue queue);

    void startDebugLabel(const VkCommandBuffer cmdBuffer, const std::string& name);
    void endDebugLabel(const VkCommandBuffer cmdBuffer);

    /*
    =========
    descriptors and layouts
    =========
    */
    VkDescriptorSet m_globalDescriptorSet;     

    //discriptor pools are added as existing ones run out
    std::vector<DescriptorPool> m_descriptorPools;

    //the imgui pool is just passed to the library, no need for allocation counting
    VkDescriptorPool m_imguiDescriptorPool;

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

    VkDescriptorSetLayout m_globalDescriptorSetLayout;    //contains global info, always bound to set 0

    DescriptorPoolAllocationSizes descriptorSetAllocationSizeFromShaderReflection(const ShaderReflection& reflection);
    DescriptorPoolAllocationSizes descriptorSetAllocationSizeFromMaterialFlags(const MaterialFeatureFlags& flags);

    //creates new descriptor pool if needed
    //currently now way to free descriptor set
    VkDescriptorSet         allocateDescriptorSet(const VkDescriptorSetLayout setLayout, const DescriptorPoolAllocationSizes& requiredSizes);
    void                    updateDescriptorSet(const VkDescriptorSet set, const RenderPassResources& resources);
    VkDescriptorSetLayout   createDescriptorSetLayout(const ShaderLayout& shaderLayout);

    /*
    materialSetLayout may be VK_NULL_HANDLE, this is the case for compute passes
    isGraphicsPass controls if the push constant range is setup for the MVP matrix
    */
    VkPipelineLayout        createPipelineLayout(const VkDescriptorSetLayout setLayout, const VkDescriptorSetLayout materialSetLayout, 
        const bool isGraphicPass);


    /*
    =========
    renderpass creation
    =========
    */

    /*
    actual creation of internal objects
    split from public function to allow use when reloading shader
    */
    ComputePass createComputePassInternal(const ComputePassDescription& desc, const std::vector<uint32_t>& spirV);
    GraphicPass createGraphicPassInternal(const GraphicPassDescription& desc, const GraphicPassShaderSpirV& spirV);

    VkRenderPass    createVulkanRenderPass(const std::vector<Attachment>& attachments);
    VkFramebuffer   createFramebuffer(const VkRenderPass renderPass, const VkExtent2D extent, const std::vector<Attachment>& attachments);
    VkShaderModule  createShaderModule(const std::vector<uint32_t>& code);

    //outAdditionalInfo has to be from parent scope to keep pointers to info structs valid
    VkPipelineShaderStageCreateInfo         createPipelineShaderStageInfos(const VkShaderModule module, const VkShaderStageFlagBits stage, 
        const ShaderSpecialisationConstants& specialisationInfo, VulkanShaderCreateAdditionalStructs* outAdditionalInfo);
    VkPipelineInputAssemblyStateCreateInfo  createDefaultInputAssemblyInfo();
    VkPipelineTessellationStateCreateInfo   createTesselationState(const uint32_t patchControlPoints);
    VkPipelineRasterizationStateCreateInfo  createRasterizationState(const RasterizationConfig& raster);
    VkPipelineMultisampleStateCreateInfo    createDefaultMultisamplingInfo();
    VkPipelineDepthStencilStateCreateInfo   createDepthStencilState(const DepthTest& depthTest);

    /*
    renderpass creation utilities
    */
    bool isDepthFormat(VkFormat format);
    bool isDepthFormat(ImageFormat format);

    /*
    renderpass barriers
    */
    
    void barriersCommand(const VkCommandBuffer commandBuffer, const std::vector<VkImageMemoryBarrier>& imageBarriers, const std::vector<VkBufferMemoryBarrier>& memoryBarriers);

    void createBufferBarriers();

    /*
    create necessary image barriers
    multiple barriers may be needed, as mip levels may have differing layouts
    sets image.layout to newLayout, image.currentAccess to dstAccess and image.currentlyWriting to false
    */
    std::vector<VkImageMemoryBarrier> createImageBarriers(Image& image, const VkImageLayout newLayout,
        const VkAccessFlags dstAccess, const uint32_t baseMip, const uint32_t mipLevels);

    VkBufferMemoryBarrier   createBufferBarrier(const Buffer& buffer, const VkAccessFlagBits srcAccess, const VkAccessFlagBits dstAccess);

    /*
    =========
    sync objects
    =========
    */
    VkSemaphore m_renderFinishedSemaphore;
    VkFence     m_renderFinishedFence;


    VkSemaphore createSemaphore();
    VkFence     createFence();


    /*
    =========
    resource destruction
    =========
    */
    void destroyImage(const ImageHandle handle);
    void destroyBuffer(const Buffer& buffer);
    void destroyMesh(const Mesh& mesh);
    void destroyDynamicMesh(const DynamicMesh& mesh);
    void destroyGraphicPass(const GraphicPass& pass);
    void destroyComputePass(const ComputePass& pass);
};