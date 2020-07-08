#include "pch.h"
#include "ImageLoader.h"

#include "Utilities/DirectoryUtils.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

ImageDescription loadImage(const std::filesystem::path& path, const bool isFullPath) {

    std::filesystem::path fullPath;
    if (isFullPath) {
        fullPath = path;
    }
    else {
        fullPath = DirectoryUtils::getResourceDirectory() / path;
    }

	int width, height, components;
    unsigned char* data;
    const bool isHdr = path.extension().string() == ".hdr";
    
    if (isHdr) {
        data = reinterpret_cast<unsigned char*>(stbi_loadf(fullPath.string().c_str(), &width, &height, &components, 0));
    }
    else {
        data = stbi_load(fullPath.string().c_str(), &width, &height, &components, 0);
    }
	 
    uint32_t bytesPerComponent = isHdr ? 4 : 1;
    size_t dataSize = width * height * components * bytesPerComponent; //in bytes

	if (data == nullptr) {
		std::cout << "failed to open image: " << path << std::endl;
		throw std::runtime_error("handle me"); //FIXME proper error handling
	}

    if (isHdr) {
        assert(components == 4 || components == 3); //FIXME handle all components
    }
    else {
        assert(components == 4 || components == 3 || components == 1);
    }
    

    ImageFormat format;
    if (isHdr) {
        format = ImageFormat::RGBA32_sFloat;
    }
    else {
        switch (components) {
        case(1): format = ImageFormat::R8;      break;
        case(3): format = ImageFormat::RGBA8;   break;
        case(4): format = ImageFormat::RGBA8;   break;
        default: throw std::runtime_error("unsupported image component number");
        }
    }

    ImageDescription image;
    image.width = (uint32_t)width;
    image.height = (uint32_t)height;
    image.depth = 1;
    image.mipCount = MipCount::FullChain;
    image.autoCreateMips = true;
    image.type = ImageType::Type2D;
    image.format = format;
    image.autoCreateMips = true;
    image.usageFlags = ImageUsageFlags::IMAGE_USAGE_SAMPLED;
    

    /*
    simple copy 
    */
    if (components == 4 || components == 1) {
        image.initialData.resize(dataSize);
        memcpy(image.initialData.data(), data, dataSize);
    }
    /*
    requires padding to 4 compontens
    */
    else {
        image.initialData.reserve(dataSize * 1.25);
        if (isHdr) {
            for (int i = 0; i < dataSize; i += 12) {
                /*
                12 bytes data for rgb
                */
                for (int j = 0; j < 12; j++) {
                    image.initialData.push_back(data[i + j]);
                }
                /*
                4 byte padding for alpha channel
                */
                for (int j = 0; j < 4; j++) {
                    image.initialData.push_back(1);
                }
            }
        }
        else{
            for (int i = 0; i < dataSize; i += 3) {
                /*
                3 bytes data for rgb
                */
                image.initialData.push_back(data[i]);
                image.initialData.push_back(data[i + 1]);
                image.initialData.push_back(data[i + 2]);
                /*
                single byte padding for alpha
                */
                image.initialData.push_back(1);
            }
        }
    }
    

	stbi_image_free(data);
	return image;
}