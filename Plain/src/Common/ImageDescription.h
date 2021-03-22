#pragma once
#include "pch.h"

enum class ImageType { Type1D, Type2D, Type3D, TypeCube };
enum class MipCount { One, FullChain, Manual, FullChainAlreadyInData };

enum class ImageUsageFlags : uint32_t {
    Storage = 0x00000001,
    Sampled = 0x00000002,
    Attachment = 0x00000004,
};

ImageUsageFlags operator&(const ImageUsageFlags l, const ImageUsageFlags r);
ImageUsageFlags operator|(const ImageUsageFlags l, const ImageUsageFlags r);

enum class ImageFormat { R8, RG8, RGBA8, R16_sFloat, RG16_sFloat, RG32_sFloat, RG16_sNorm, RGBA16_sFloat, RGBA16_sNorm, RGBA32_sFloat, R11G11B10_uFloat, Depth16, Depth32, BC1, BC3, BC5, BGRA8_uNorm };

struct ImageDescription {
    uint32_t width = 1;
    uint32_t height = 0;
    uint32_t depth = 0;

    ImageType       type = ImageType::Type1D;
    ImageFormat     format = ImageFormat::R8;
    ImageUsageFlags usageFlags = (ImageUsageFlags)0;

    MipCount    mipCount = MipCount::One;
    uint32_t    manualMipCount = 1; //only used if mipCount is Manual
    bool        autoCreateMips = false;
};

//result is a float because compressed formats can have less than one byte per pixel
float getImageFormatBytePerPixel(const ImageFormat format);
bool getImageFormatIsBCnCompressed(const ImageFormat format);