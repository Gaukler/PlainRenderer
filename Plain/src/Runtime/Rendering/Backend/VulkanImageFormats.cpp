#include "pch.h"
#include "VulkanImageFormats.h"

VkFormat imageFormatToVulkanFormat(const ImageFormat format) {
    switch (format) {
    case ImageFormat::R8:               return VK_FORMAT_R8_UNORM;
    case ImageFormat::RG8:              return VK_FORMAT_R8G8_UNORM;
    case ImageFormat::RGBA8:            return VK_FORMAT_R8G8B8A8_UNORM;
    case ImageFormat::RG16_sFloat:      return VK_FORMAT_R16G16_SFLOAT;
    case ImageFormat::RG32_sFloat:      return VK_FORMAT_R32G32_SFLOAT;
    case ImageFormat::R16_sFloat:       return VK_FORMAT_R16_SFLOAT;
    case ImageFormat::RG16_sNorm:       return VK_FORMAT_R16G16_SNORM;
    case ImageFormat::RGBA16_sFloat:    return VK_FORMAT_R16G16B16A16_SFLOAT;
    case ImageFormat::RGBA16_sNorm:     return VK_FORMAT_R16G16B16A16_SNORM;
    case ImageFormat::R11G11B10_uFloat: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case ImageFormat::RGBA32_sFloat:    return VK_FORMAT_R32G32B32A32_SFLOAT;
    case ImageFormat::Depth16:          return VK_FORMAT_D16_UNORM;
    case ImageFormat::Depth32:          return VK_FORMAT_D32_SFLOAT;
    case ImageFormat::BC1:              return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
    case ImageFormat::BC3:              return VK_FORMAT_BC3_UNORM_BLOCK;
    case ImageFormat::BC5:              return VK_FORMAT_BC5_UNORM_BLOCK;
    case ImageFormat::BGRA8_uNorm:            return VK_FORMAT_B8G8R8A8_UNORM;
    default: std::cout << "Unknown Image format\n"; return VK_FORMAT_MAX_ENUM;
    }
}

VkImageAspectFlagBits imageFormatToVkAspectFlagBits(const ImageFormat format) {
    switch (format) {
    case ImageFormat::R8:               return VK_IMAGE_ASPECT_COLOR_BIT;
    case ImageFormat::RG8:              return VK_IMAGE_ASPECT_COLOR_BIT;
    case ImageFormat::RGBA8:            return VK_IMAGE_ASPECT_COLOR_BIT;
    case ImageFormat::RG16_sFloat:      return VK_IMAGE_ASPECT_COLOR_BIT;
    case ImageFormat::RG32_sFloat:      return VK_IMAGE_ASPECT_COLOR_BIT;
    case ImageFormat::R16_sFloat:       return VK_IMAGE_ASPECT_COLOR_BIT;
    case ImageFormat::RG16_sNorm:       return VK_IMAGE_ASPECT_COLOR_BIT;
    case ImageFormat::RGBA16_sFloat:    return VK_IMAGE_ASPECT_COLOR_BIT;
    case ImageFormat::RGBA16_sNorm:     return VK_IMAGE_ASPECT_COLOR_BIT;
    case ImageFormat::R11G11B10_uFloat: return VK_IMAGE_ASPECT_COLOR_BIT;
    case ImageFormat::RGBA32_sFloat:    return VK_IMAGE_ASPECT_COLOR_BIT;
    case ImageFormat::Depth16:          return VK_IMAGE_ASPECT_DEPTH_BIT;
    case ImageFormat::Depth32:          return VK_IMAGE_ASPECT_DEPTH_BIT;
    case ImageFormat::BC1:              return VK_IMAGE_ASPECT_COLOR_BIT;
    case ImageFormat::BC3:              return VK_IMAGE_ASPECT_COLOR_BIT;
    case ImageFormat::BC5:              return VK_IMAGE_ASPECT_COLOR_BIT;
    case ImageFormat::BGRA8_uNorm:            return VK_IMAGE_ASPECT_COLOR_BIT;
    default: std::cout << "Unknown Image format\n"; return VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM;
    }
}

ImageFormat vulkanImageFormatToImageFormat(const VkFormat format) {
    switch (format) {
    case VK_FORMAT_R8_UNORM:                return ImageFormat::R8;
    case VK_FORMAT_R8G8_UNORM:              return ImageFormat::RG8;
    case VK_FORMAT_R8G8B8A8_UNORM:          return ImageFormat::RGBA8;
    case VK_FORMAT_R16G16_SFLOAT:           return ImageFormat::RG16_sFloat;
    case VK_FORMAT_R32G32_SFLOAT:           return ImageFormat::RG32_sFloat;
    case VK_FORMAT_R16_SFLOAT:              return ImageFormat::R16_sFloat;
    case VK_FORMAT_R16G16_SNORM:            return ImageFormat::RG16_sNorm;
    case VK_FORMAT_R16G16B16A16_SFLOAT:     return ImageFormat::RGBA16_sFloat;
    case VK_FORMAT_R16G16B16A16_SNORM:      return ImageFormat::RGBA16_sNorm;
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return ImageFormat::R11G11B10_uFloat;
    case VK_FORMAT_R32G32B32A32_SFLOAT:     return ImageFormat::RGBA32_sFloat;
    case VK_FORMAT_D16_UNORM:               return ImageFormat::Depth16;
    case VK_FORMAT_D32_SFLOAT:              return ImageFormat::Depth32;
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:     return ImageFormat::BC1;
    case VK_FORMAT_BC3_UNORM_BLOCK:         return ImageFormat::BC3;
    case VK_FORMAT_BC5_UNORM_BLOCK:         return ImageFormat::BC5;
    case VK_FORMAT_B8G8R8A8_UNORM:          return ImageFormat::BGRA8_uNorm;
    default: std::cout << "Unknown Image format\n"; return ImageFormat::R8;
    }
}