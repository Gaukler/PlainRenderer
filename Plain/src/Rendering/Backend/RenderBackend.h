#pragma once
#include "pch.h"
#include <vulkan/vulkan.h>

#include "Rendering/RenderHandles.h"
#include "Rendering/ResourceDescriptions.h"
#include "Resources.h"
#include "Rendering/MeshData.h"

struct GLFWwindow;

struct QueueFamilies {
    uint32_t graphicsQueueIndex;
    uint32_t presentationQueueIndex;
    uint32_t computeQueueIndex;
    uint32_t transferQueueFamilyIndex;
};

struct RenderContext {
    VkInstance vulkanInstance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    QueueFamilies queueFamilies;
    VkQueue graphicQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;
};

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
    void drawMesh(const MeshHandle meshHandle, const std::vector<RenderPassHandle>& passes, const glm::mat4& modelMatrix);

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

    MeshHandle          createMesh(const MeshData& data, const std::vector<RenderPassHandle>& passes);
    ImageHandle         createImage(const ImageDescription& description);
    UniformBufferHandle createUniformBuffer(const BufferDescription& description);
    StorageBufferHandle createStorageBuffer(const BufferDescription& description);
    SamplerHandle       createSampler(const SamplerDescription& description);

    void                setSwapchainInputImage(ImageHandle image);

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
    void submitRenderPass(const RenderPassExecutionInternal& execution);

    void waitForRenderFinished();

    /*
    =========
    context
    =========
    */
    RenderContext               m_context;
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
    std::vector<RenderPass>     m_renderPasses;
    std::vector<Image>          m_images;
    std::vector<Mesh>           m_meshes;
    std::vector<VkSampler>      m_samplers;
    std::vector<Buffer>         m_uniformBuffers;
    std::vector<Buffer>         m_storageBuffers;

    /*
    freed indices
    */
    std::vector<ImageHandle>    m_freeImageHandles;

    UniformBufferHandle m_globalShaderInfoBuffer;

    VkDeviceMemory      allocateMemory(const VkMemoryRequirements& requirements, const VkMemoryPropertyFlags flags);

    VkImageView createImageView(const Image image, const VkImageViewType viewType, const uint32_t baseMip, const uint32_t mipLevels, const VkImageAspectFlags aspectMask);
    Buffer      createBufferInternal(const VkDeviceSize size, const std::vector<uint32_t>& queueFamilies, const VkBufferUsageFlags usage, const uint32_t memoryFlags);

    VkImageSubresourceLayers createSubresourceLayers(const Image& image, const uint32_t mipLevel);

    /*
    copies data into a temporary staging buffer, then transfers data into image
    */
    void transferDataIntoImage(Image& target, const void* data, const VkDeviceSize size);
    void generateMipChain(Image& image, const VkImageLayout oldLayout, const VkImageLayout newLayout);

    /*
    fills buffer using a temporary staging buffer
    uses createStagingBuffer then copyBuffer and cleans everything up
    */
    void fillBuffer(Buffer target, const void* data, const VkDeviceSize size);

    /*
    create a staging buffer
    data is copied into buffer, can be used to copy transfer data into resource
    */
    Buffer createStagingBuffer(const void* data, const VkDeviceSize size);

    /*
    buffers must have been allocated using transfer src/dst bit
    */
    void copyBuffer(const Buffer src, const Buffer dst, const VkDeviceSize size);


    /*
    =========
    commands 
    =========
    */
    VkCommandPool   m_commandPool;
    VkCommandPool   m_transientCommandPool; //used for short lived copy command buffer and such
    VkCommandBuffer m_commandBuffer;        //primary command buffer used for all rendering

    
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


    /*
    =========
    descriptors and layouts
    =========
    */
    VkDescriptorSet m_globalDescriptorSet;     

    /*
    FIXME make sure desciptor pool doesn't run out of allocations
    */
    VkDescriptorPool        m_descriptorPool;
    VkDescriptorSetLayout   m_globalDescriptorSetLayout;    //contains global info, always bound to set 0

    VkDescriptorPool        createDescriptorPool(const uint32_t maxSets);
    VkDescriptorSet         allocateDescriptorSet(const VkDescriptorSetLayout setLayout);
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
    RenderPass createComputePassInternal(const ComputePassDescription& desc, const std::vector<uint32_t>& spirV);
    RenderPass createGraphicPassInternal(const GraphicPassDescription& desc, const GraphicPassShaderSpirV& spirV);

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
    VkSemaphore             m_renderFinished;
    VkFence                 m_imageInFlight;


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
    void destroyRenderPass(const RenderPass& pass);
};