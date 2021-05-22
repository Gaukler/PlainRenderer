#include "pch.h"
#include "VulkanContext.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#ifdef USE_VK_VALIDATION_LAYERS
const bool gUseValidationLayers = true;
#else
const bool gUseValidationLayers = false;
#endif

VkDebugReportCallbackEXT gDebugCallback = VK_NULL_HANDLE;

// in release mode the function is empty, resulting in a warning
// disable warning for the function
#pragma warning( push )
#pragma warning( disable : 4100 ) // unreferenced formal parameter

void checkVulkanResult(const VkResult result) {
#ifndef NDEBUG
    if (result != VK_SUCCESS) {
        std::cout << "Vulkan function failed\n";
    }
#endif 
}

// reenable warning
#pragma warning( pop )

std::vector<const char*> getRequiredInstanceExtensions() {

    // query required glfw extensions
    uint32_t requiredExtensionGlfwCount = 0;
    const char** requiredExtensionsGlfw = glfwGetRequiredInstanceExtensions(&requiredExtensionGlfwCount);

    // add debug extension if used
    std::vector<const char*> requestedExtensions(requiredExtensionsGlfw, requiredExtensionsGlfw + requiredExtensionGlfwCount);
    requestedExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (gUseValidationLayers) {
        requestedExtensions.push_back("VK_EXT_debug_report");
    }

    return requestedExtensions;
}

void createVulkanInstance() {

    // retrieve and print requested extensions
    std::vector<const char*> requestedExtensions = getRequiredInstanceExtensions();
    std::cout << "requested extensions: " << std::endl;
    for (const auto ext : requestedExtensions) {
        std::cout << ext << std::endl;
    }
    std::cout << std::endl;

    // list avaible extensions
    uint32_t avaibleExtensionCount = 0;
    auto res = vkEnumerateInstanceExtensionProperties(nullptr, &avaibleExtensionCount, nullptr);
    assert(res == VK_SUCCESS);
    std::vector<VkExtensionProperties> avaibleExtensions(avaibleExtensionCount);
    res = vkEnumerateInstanceExtensionProperties(nullptr, &avaibleExtensionCount, avaibleExtensions.data());
    assert(res == VK_SUCCESS);

    std::cout << "avaible instance extensions: " << std::endl;
    for (const auto& ext : avaibleExtensions) {
        std::cout << ext.extensionName << std::endl;
    }
    std::cout << std::endl;

    // ensure all required extensions are avaible
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
            throw std::runtime_error("required instance extension not avaible: " + required);
        }
    }

    // list avaible layers
    uint32_t avaibleLayerCount = 0;
    res = vkEnumerateInstanceLayerProperties(&avaibleLayerCount, nullptr);
    assert(res == VK_SUCCESS);
    std::vector<VkLayerProperties> avaibleLayers(avaibleLayerCount);
    res = vkEnumerateInstanceLayerProperties(&avaibleLayerCount, avaibleLayers.data());
    assert(res == VK_SUCCESS);

    std::cout << "avaible layers" << std::endl;
    for (const auto& avaible : avaibleLayers) {
        std::cout << avaible.layerName << std::endl;
    }
    std::cout << std::endl;

    // validation layers
    std::vector<const char*> requestedLayers;
    if (gUseValidationLayers) {
        requestedLayers.push_back("VK_LAYER_KHRONOS_validation");
    }

    // ensure all requested layers are avaible
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

    // create instance
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

    res = vkCreateInstance(&instanceInfo, nullptr, &vkContext.vulkanInstance);
    checkVulkanResult(res);

    if (gUseValidationLayers) {
        gDebugCallback = setupDebugCallbacks();
    }
}

bool hasRequiredDeviceFeatures(const VkPhysicalDevice physicalDevice) {
    // check features
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);

    VkPhysicalDeviceVulkan12Features features12;
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = nullptr;

    VkPhysicalDeviceFeatures2 features2;
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features12;

    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

    const bool supportsRequiredFeatures =
        features.samplerAnisotropy &&
        features.imageCubeArray &&
        features.fragmentStoresAndAtomics &&
        features.fillModeNonSolid &&
        features.depthClamp &&
        features.geometryShader &&
        features12.hostQueryReset &&
        features12.runtimeDescriptorArray &&
        features12.descriptorBindingPartiallyBound &&
        features12.descriptorBindingVariableDescriptorCount;

    // check device extensions
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());

    bool supportsDeviceExtensions = false;

    // currently only requiring conservative rasterisation
    for (const VkExtensionProperties& ext : extensions) {
        if (strcmp(ext.extensionName, VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME)) {
            supportsDeviceExtensions = true;
            break;
        }
    }

    return supportsRequiredFeatures && supportsDeviceExtensions;
}

void pickPhysicalDevice(const VkSurfaceKHR surface) {

    // enumerate devices
    uint32_t deviceCount = 0;
    auto res = vkEnumeratePhysicalDevices(vkContext.vulkanInstance, &deviceCount, nullptr);
    assert(res == VK_SUCCESS);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    res = vkEnumeratePhysicalDevices(vkContext.vulkanInstance, &deviceCount, devices.data());
    assert(res == VK_SUCCESS);

    // pick first suitable device
    for (const auto& device : devices) {
        QueueFamilies families;
        if (getQueueFamilies(device, &families, surface) && hasRequiredDeviceFeatures(device)) {
            vkContext.physicalDevice = device;
            break;
        }
    }

    if (vkContext.physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find suitable physical device");
    }
}

bool getQueueFamilies(const VkPhysicalDevice device, QueueFamilies* pOutQueueFamilies, const VkSurfaceKHR surface) {

    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

    bool foundCompute = false;
    bool foundGraphics = false;
    bool foundPresentation = false;

    // iterate families and check if they fit our needs
    for (uint32_t i = 0; i < familyCount; i++) {
        if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            pOutQueueFamilies->computeQueueIndex = i;
            foundCompute = true;
        }
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            pOutQueueFamilies->graphicsQueueIndex = i;
            pOutQueueFamilies->transferQueueFamilyIndex = i;
            foundGraphics = true;
        }

        VkBool32 isPresentationQueue;
        auto res = vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &isPresentationQueue);
        checkVulkanResult(res);

        if (isPresentationQueue) {
            pOutQueueFamilies->presentationQueueIndex = i;
            foundPresentation = true;
        }
    }
    return foundCompute && foundGraphics && foundPresentation;
}

void createLogicalDevice() {

    // set removes duplicates
    std::set<uint32_t> uniqueQueueFamilies = {
        vkContext.queueFamilies.graphicsQueueIndex,
        vkContext.queueFamilies.computeQueueIndex,
        vkContext.queueFamilies.presentationQueueIndex,
        vkContext.queueFamilies.transferQueueFamilyIndex
    };

    // queue infos
    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    // TODO make queues unique if possible
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
    features.fragmentStoresAndAtomics = true;
    features.fillModeNonSolid = true;
    features.depthClamp = true;
    features.geometryShader = true;

    VkPhysicalDeviceVulkan12Features features12 = {}; //vulkan 1.2 features
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = nullptr;
    features12.hostQueryReset = true;
    features12.runtimeDescriptorArray = true;
    features12.descriptorBindingPartiallyBound = true;
    features12.descriptorBindingVariableDescriptorCount = true;

    // device info
    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &features12;
    deviceInfo.flags = 0;
    deviceInfo.queueCreateInfoCount = (uint32_t)queueInfos.size();
    deviceInfo.pQueueCreateInfos = queueInfos.data();
    deviceInfo.enabledLayerCount = 0;           // depreceated and ignored
    deviceInfo.ppEnabledLayerNames = nullptr;   // depreceated and ignored
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.pEnabledFeatures = &features;

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;

    auto res = vkCreateDevice(vkContext.physicalDevice, &deviceInfo, nullptr, &vkContext.device);
    checkVulkanResult(res);
}

// callback needs a lot of parameters which are not used
// disable warning for this function
#pragma warning( push )
#pragma warning( disable : 4100 ) // unreference formal parameter

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

// reenable warnings
#pragma warning( pop )

VkDebugReportCallbackEXT setupDebugCallbacks() {

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
        (vkGetInstanceProcAddr(vkContext.vulkanInstance, "vkCreateDebugReportCallbackEXT"));

    VkDebugReportCallbackEXT debugCallback;
    auto res = vkCreateDebugReportCallbackEXT(vkContext.vulkanInstance, &callbackInfo, nullptr, &debugCallback);
    checkVulkanResult(res);

    return debugCallback;
}

void destroyVulkanInstance() {
    if (gUseValidationLayers) {
        PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
            reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>
            (vkGetInstanceProcAddr(vkContext.vulkanInstance, "vkDestroyDebugReportCallbackEXT"));
        vkDestroyDebugReportCallbackEXT(vkContext.vulkanInstance, gDebugCallback, nullptr);
    }
    vkDestroyInstance(vkContext.vulkanInstance, nullptr);
}

VkPhysicalDeviceProperties getVulkanDeviceProperties() {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(vkContext.physicalDevice, &deviceProperties);
    return deviceProperties;
}

void initializeVulkanQueues() {
    vkGetDeviceQueue(vkContext.device, vkContext.queueFamilies.graphicsQueueIndex, 0, &vkContext.graphicQueue);
    vkGetDeviceQueue(vkContext.device, vkContext.queueFamilies.presentationQueueIndex, 0, &vkContext.presentQueue);
    vkGetDeviceQueue(vkContext.device, vkContext.queueFamilies.transferQueueFamilyIndex, 0, &vkContext.transferQueue);
    vkGetDeviceQueue(vkContext.device, vkContext.queueFamilies.computeQueueIndex, 0, &vkContext.computeQueue);
}

void waitForGpuIdle() {
    const auto result = vkDeviceWaitIdle(vkContext.device);
    checkVulkanResult(result);
}