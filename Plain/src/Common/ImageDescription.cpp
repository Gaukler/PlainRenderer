#include "pch.h"
#include "ImageDescription.h"

//enum class bit operators
ImageUsageFlags operator&(const ImageUsageFlags l, const ImageUsageFlags r) {
    return ImageUsageFlags(uint32_t(l) & uint32_t(r));
}

ImageUsageFlags operator|(const ImageUsageFlags l, const ImageUsageFlags r) {
    return ImageUsageFlags(uint32_t(l) | uint32_t(r));
}

bool isDepthFormat(ImageFormat format) {
    return (format == ImageFormat::Depth16 ||
        format == ImageFormat::Depth32);
}