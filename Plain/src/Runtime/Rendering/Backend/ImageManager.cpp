#include "pch.h"
#include "ImageManager.h"
#include "VulkanContext.h"

ImageManager instance;

ImageManager& ImageManager::getRef() {
    return instance;
}

struct ImageHandleDecoded {
    bool isTempImage = false;
    int index = 0;
};

ImageHandleDecoded decodeImageHandle(const ImageHandle handle) {
    ImageHandleDecoded decoded;
    decoded.isTempImage = bool(handle.index >> 31); //first bit indicates if image is temp
    decoded.index = handle.index & 0x7FFFFFFF;      //mask out first bit for index
    return decoded;
}

Image& ImageManager::getImageRef(const ImageHandle handle) {
    const ImageHandleDecoded decoded = decodeImageHandle(handle);
    if (decoded.isTempImage) {
        const int allocationIndex = m_temporaryImages[decoded.index].allocationIndex;
        assert(allocationIndex >= allocationIndex);
        assert(allocationIndex < m_allocatedTempImages.size());
        return m_allocatedTempImages[allocationIndex].image;
    }
    else {
        return m_images[decoded.index];
    }
}

bool imageDescriptionsMatch(const ImageDescription& d1, const ImageDescription& d2) {
    return
        d1.format == d2.format &&
        d1.width == d2.width &&
        d1.height == d2.height &&
        d1.depth == d2.depth &&
        d1.mipCount == d2.mipCount &&
        d1.type == d2.type &&
        d1.usageFlags == d2.usageFlags;
}

void ImageManager::mapOverRenderpassTempImages(std::function<void(const int renderpassImage,
    const int tempImageIndex)> function) {

    for (int i = 0; i < m_renderPassExecutions.size(); i++) {
        const auto& executionEntry = m_renderPassExecutions[i];
        const RenderPassExecution& execution = getGenericRenderpassInfoFromExecutionEntry(executionEntry,
            m_graphicPassExecutions, m_computePassExecutions);

        for (const auto& imageResource : execution.resources.sampledImages) {
            const auto decodedImageHandle = decodeImageHandle(imageResource.image);
            if (decodedImageHandle.isTempImage) {
                function(i, decodedImageHandle.index);
            }
        }

        for (const auto& imageResource : execution.resources.storageImages) {
            const auto decodedImageHandle = decodeImageHandle(imageResource.image);
            if (decodedImageHandle.isTempImage) {
                function(i, decodedImageHandle.index);
            }
        }
        if (executionEntry.type == RenderPassType::Graphic) {
            const auto& graphicPass = m_graphicPassExecutions[executionEntry.index];
            for (const auto& target : graphicPass.targets) {
                const auto decodedImageHandle = decodeImageHandle(target.image);
                if (decodedImageHandle.isTempImage) {
                    function(i, decodedImageHandle.index);
                }
            }
        }
    }
}

void ImageManager::allocateTemporaryImages() {

    //compute temp image usage
    struct TempImageUsage {
        int firstUse = std::numeric_limits<int>::max();
        int lastUse = 0;
    };
    std::vector<TempImageUsage> imagesUsage(m_temporaryImages.size());

    std::function<void(const int, const int)> usageLambda = [&imagesUsage](const int renderpassIndex, const int tempImageIndex) {
        TempImageUsage& usage = imagesUsage[tempImageIndex];
        usage.firstUse = std::min(usage.firstUse, renderpassIndex);
        usage.lastUse = std::max(usage.lastUse, renderpassIndex);
    };

    mapOverRenderpassTempImages(usageLambda);

    std::vector<int> allocatedImageLatestUsedPass(m_allocatedTempImages.size(), 0);

    std::function<void(const int, const int)> allocationLambda =
        [&imagesUsage, &allocatedImageLatestUsedPass, this](const int renderPassIndex, const int tempImageIndex) {

        auto& tempImage = m_temporaryImages[tempImageIndex];

        const bool isAlreadyAllocated = tempImage.allocationIndex >= 0;
        if (isAlreadyAllocated) {
            return;
        }

        bool foundAllocatedImage = false;
        const TempImageUsage& usage = imagesUsage[tempImageIndex];

        for (int allocatedImageIndex = 0; allocatedImageIndex < m_allocatedTempImages.size(); allocatedImageIndex++) {

            int& allocatedImageLastUse = allocatedImageLatestUsedPass[allocatedImageIndex];
            auto& allocatedImage = m_allocatedTempImages[allocatedImageIndex];
            const bool allocatedImageAvailable = allocatedImageLastUse < renderPassIndex;

            const bool requirementsMatching = imageDescriptionsMatch(tempImage.desc, allocatedImage.image.desc);
            if (allocatedImageAvailable && requirementsMatching) {
                tempImage.allocationIndex = allocatedImageIndex;
                allocatedImageLastUse = usage.lastUse;
                foundAllocatedImage = true;
                allocatedImage.usedThisFrame = true;
                break;
            }
        }
        if (!foundAllocatedImage) {
            std::cout << "Allocated temp image\n";
            AllocatedTempImage allocatedImage;
            allocatedImage.image = createImageInternal(tempImage.desc, nullptr, 0);
            allocatedImage.usedThisFrame = true;
            tempImage.allocationIndex = m_allocatedTempImages.size();
            m_allocatedTempImages.push_back(allocatedImage);
            allocatedImageLatestUsedPass.push_back(usage.lastUse);
        }
    };
    mapOverRenderpassTempImages(allocationLambda);
}

void ImageManager::resetAllocatedTempImages() {
    for (int i = 0; i < m_allocatedTempImages.size(); i++) {
        if (!m_allocatedTempImages[i].usedThisFrame) {
            //delete unused image
            std::swap(m_allocatedTempImages.back(), m_allocatedTempImages[i]);
            vkDeviceWaitIdle(vkContext.device); //FIXME: don't use wait idle, use deferred destruction queue instead
            destroyImageInternal(m_allocatedTempImages.back().image);
            m_allocatedTempImages.pop_back();
            std::cout << "Deleted unused image\n";
        }
        else {
            //reset usage
            m_allocatedTempImages[i].usedThisFrame = false;
        }
    }
}

void ImageManager::destroyImage(const ImageHandle handle) {
    m_freeImageHandles.push_back(handle);
    const Image& image = getImageRef(handle);
    destroyImageInternal(image);
}

void ImageManager::destroyImageInternal(const Image& image) {

    if (bool(image.desc.usageFlags & ImageUsageFlags::Sampled)) {
        m_globalTextureArrayDescriptorSetFreeTextureIndices.push_back(image.globalDescriptorSetIndex);
    }

    for (const auto& view : image.viewPerMip) {
        vkDestroyImageView(vkContext.device, view, nullptr);
    }

    //swapchain images have no manualy allocated memory
    //they are deleted by the swapchain
    //view has to be destroyed manually though
    if (!image.isSwapchainImage) {
        m_vkAllocator.free(image.memory);
        vkDestroyImage(vkContext.device, image.vulkanHandle, nullptr);
    }
}