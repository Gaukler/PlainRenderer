#include "pch.h"
#include "ImageDescription.h"

float getImageFormatBytePerPixel(const ImageFormat format) {
    if (format == ImageFormat::R8) {
        return 1.f;
    }
    else if (format == ImageFormat::R11G11B10_uFloat) {
        return 4;
    }
    else if (format == ImageFormat::RG16_sFloat) {
        return 4;
    }
    else if (format == ImageFormat::RG32_sFloat) {
        return 8;
    }
    else if (format == ImageFormat::RG8) {
        return 2;
    }
    else if (format == ImageFormat::R16_sFloat) {
        return 2;
    }
    else if (format == ImageFormat::RGBA16_sFloat) {
        return 8;
    }
    else if (format == ImageFormat::RGBA16_sNorm) {
        return 8;
    }
    else if (format == ImageFormat::RGBA32_sFloat) {
        return 16;
    }
    else if (format == ImageFormat::RGBA8) {
        return 4;
    }
    else if (format == ImageFormat::BC1) {
        return 0.5;
    }
    else if (format == ImageFormat::BC3) {
        return 1;
    }
    else if (format == ImageFormat::BC5) {
        return 1;
    }
    else {
        throw("Unsupported format");
    }
}

bool getImageFormatIsBCnCompressed(const ImageFormat format) {
    if (format == ImageFormat::BC1) {
        return true;
    }
    else if (format == ImageFormat::BC3) {
        return true;
    }
    else if (format == ImageFormat::BC5) {
        return true;
    }
    else {
        return false;
    }
}