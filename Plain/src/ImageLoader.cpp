#include "pch.h"
#include "ImageLoader.h"

#include "Utilities/DirectoryUtils.h"

#define STB_IMAGE_IMPLEMENTATION

//disable warnings for stb_image library
#pragma warning( push )
#pragma warning( disable : 26451 26819 6011 26819 6262 6308 28182)
#include <stb_image.h>

//reenable
#pragma warning( pop )

bool loadImage(const std::filesystem::path& path, const bool isFullPath, ImageDescription* outImage) {

    std::filesystem::path fullPath;
    if (isFullPath) {
        fullPath = path;
    }
    else {
        fullPath = DirectoryUtils::getResourceDirectory() / path;        
    }

    if (path.extension().string() == ".dds") {
        return loadDDSFile(path, outImage);
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
    size_t dataSize = (size_t)width * (size_t)height * (size_t)components * (size_t)bytesPerComponent; //in bytes

	if (data == nullptr) {
		std::cout << "failed to open image: " << fullPath << std::endl;
        return false;
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
        case(2): format = ImageFormat::RG8;     break;
        case(3): format = ImageFormat::RGBA8;   break;
        case(4): format = ImageFormat::RGBA8;   break;
        default: throw std::runtime_error("unsupported image component number");
        }
    }

    outImage->width = (uint32_t)width;
    outImage->height = (uint32_t)height;
    outImage->depth = 1;
    outImage->mipCount = MipCount::FullChain;
    outImage->autoCreateMips = true;
    outImage->type = ImageType::Type2D;
    outImage->format = format;
    outImage->autoCreateMips = true;
    outImage->usageFlags = ImageUsageFlags::Sampled;

    /*
    simple copy 
    */
    if (components == 4 || components == 1 || components == 2) {
        outImage->initialData.resize(dataSize);
        memcpy(outImage->initialData.data(), data, dataSize);
    }
    /*
    requires padding to 4 compontens
    */
    else {
        outImage->initialData.reserve(size_t(dataSize * 1.25));
        if (isHdr) {
            for (int i = 0; i < dataSize; i += 12) {
                /*
                12 bytes data for rgb
                */
                for (int j = 0; j < 12; j++) {
                    outImage->initialData.push_back(data[i + j]);
                }
                /*
                4 byte padding for alpha channel
                */
                for (int j = 0; j < 4; j++) {
                    outImage->initialData.push_back(1);
                }
            }
        }
        else{
            for (int i = 0; i < dataSize; i += 3) {
                /*
                3 bytes data for rgb
                */
                outImage->initialData.push_back(data[i]);
                outImage->initialData.push_back(data[i + 1]);
                outImage->initialData.push_back(data[i + 2]);
                /*
                single byte padding for alpha
                */
                outImage->initialData.push_back(0xff); //fill with 1, otherwise section will be cut out by alpha clipping
            }
        }
    }
    

	stbi_image_free(data);
	return true;
}

//reference: https://docs.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-reference
struct DDS_PixelFormat {
    uint32_t infoSize;
    uint32_t flags;
    uint32_t compressionCode;
    uint32_t RGBBitCount;
    uint32_t RBitMask;
    uint32_t GBitMask;
    uint32_t BBitMask;
    uint32_t ABitMask;
};

struct DDS_Header {
    uint32_t        headerSize;
    uint32_t        Flags;
    uint32_t        height;
    uint32_t        width;
    uint32_t        pitchOrLinearSize;
    uint32_t        depth;
    uint32_t        mipMapCount;
    uint32_t        reserved1[11];
    DDS_PixelFormat pixelFormat;
    uint32_t        caps;
    uint32_t        caps2;
    uint32_t        caps3;
    uint32_t        caps4;
    uint32_t        reserved2;
} ;

bool loadDDSFile(const std::filesystem::path& filename, ImageDescription* outImage) {

    //open file
    std::fstream file;
    file.open(filename, std::ios::binary | std::ios::in | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "failed to open image: " << filename << std::endl;
        return false;
    }

    //file is opened at the end so current position is file size
    size_t fileSize = file.tellg();
    file.seekg(0, file.beg); //go to file start

    //validate magic number
    {
        uint32_t magicNumber;
        file.read((char*)&magicNumber, 4);
        assert(magicNumber == 0x20534444);
    }

    //read header
    DDS_Header header;
    file.read((char*)&header, sizeof(header));
    
    outImage->width = header.width;
    outImage->height = header.height;
    outImage->depth = std::max(header.depth, (uint32_t)1);
    outImage->type = ImageType::Type2D;
    outImage->mipCount = MipCount::FullChain;
    outImage->autoCreateMips = false;
    outImage->usageFlags = ImageUsageFlags::Sampled;

    /*
    only specific compressed dds formats are supported at the moment
    more formats will be added as needed
    */
    if (!(header.pixelFormat.flags & 0x4)) {
        std::cout << "Only compressed DDS files are supported: " << filename << std::endl;
        throw("Image loading error");
    }

    //find image format
    const uint32_t bc1Code = 827611204;
    const uint32_t bc3Code = 894720068;
    const uint32_t bc5Code = 843666497;
    if (header.pixelFormat.compressionCode == bc1Code) {
        outImage->format = ImageFormat::BC1;
    }
    else if (header.pixelFormat.compressionCode == bc3Code) {
        outImage->format = ImageFormat::BC3;
    }
    else if (header.pixelFormat.compressionCode == bc5Code) {
        outImage->format = ImageFormat::BC5;
    }
    else {
        std::cout << "Unsupported texture format: " << filename << std::endl;
        return false;
    }

    //data size is size of file without header and magic number
    size_t dataSize = fileSize - (sizeof(header) + sizeof(uint32_t));

    //copy data
    outImage->initialData.resize(dataSize);
    file.read((char*)outImage->initialData.data(), dataSize);

    file.close();
    return true;
}