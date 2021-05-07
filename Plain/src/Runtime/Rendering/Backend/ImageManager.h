#pragma once
#include "pch.h"
#include "Resources.h"
#include "Runtime/Rendering/RenderHandles.h"

class ImageManager {
public:
    static ImageManager& getRef();
    ImageManager(const ImageManager&) = delete; // no copy

    Image& getImageRef(const ImageHandle handle);
    ImageHandle addImage(const Image& image);

    void allocateTemporaryImages();
    void resetAllocatedTempImages();

    // temporary images are the preferred way to create render targets and other temp images
    // they are valid for one frame
    // their lifetime is automatically managed and existing images are reused where possible
    ImageHandle createTemporaryImage(const ImageDescription& description);

    void destroyImage(const ImageHandle handle);

private:
    ImageManager();

    void destroyImageInternal(const Image& image);

    struct TemporaryImage {
        ImageDescription desc;
        int allocationIndex = -1;
    };

    std::vector<TemporaryImage> m_temporaryImages;

    struct AllocatedTempImage {
        bool usedThisFrame = false;
        Image image;
    };

    std::vector<AllocatedTempImage> m_allocatedTempImages;  //allocated images are shared by non-overlapping temporary images

    std::vector<ImageHandle> m_freeImageHandles;
    std::vector<Image> m_images;
};