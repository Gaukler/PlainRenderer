#include "pch.h"
#include "ImageIO.h"
#include "Utilities/DirectoryUtils.h"
#include "FileIO.h"
#include "Utilities/MathUtils.h"

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

DDS_PixelFormat getDDSPixelFormat();

struct DDS_header {
    uint32_t        headerSize;
    uint32_t        flags;
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
};

enum DXGI_FORMAT {
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_R32G32B32A32_TYPELESS,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_UINT,
    DXGI_FORMAT_R32G32B32A32_SINT,
    DXGI_FORMAT_R32G32B32_TYPELESS,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32_UINT,
    DXGI_FORMAT_R32G32B32_SINT,
    DXGI_FORMAT_R16G16B16A16_TYPELESS,
    DXGI_FORMAT_R16G16B16A16_FLOAT,
    DXGI_FORMAT_R16G16B16A16_UNORM,
    DXGI_FORMAT_R16G16B16A16_UINT,
    DXGI_FORMAT_R16G16B16A16_SNORM,
    DXGI_FORMAT_R16G16B16A16_SINT,
    DXGI_FORMAT_R32G32_TYPELESS,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32_UINT,
    DXGI_FORMAT_R32G32_SINT,
    DXGI_FORMAT_R32G8X24_TYPELESS,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT,
    DXGI_FORMAT_R10G10B10A2_TYPELESS,
    DXGI_FORMAT_R10G10B10A2_UNORM,
    DXGI_FORMAT_R10G10B10A2_UINT,
    DXGI_FORMAT_R11G11B10_FLOAT,
    DXGI_FORMAT_R8G8B8A8_TYPELESS,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    DXGI_FORMAT_R8G8B8A8_UINT,
    DXGI_FORMAT_R8G8B8A8_SNORM,
    DXGI_FORMAT_R8G8B8A8_SINT,
    DXGI_FORMAT_R16G16_TYPELESS,
    DXGI_FORMAT_R16G16_FLOAT,
    DXGI_FORMAT_R16G16_UNORM,
    DXGI_FORMAT_R16G16_UINT,
    DXGI_FORMAT_R16G16_SNORM,
    DXGI_FORMAT_R16G16_SINT,
    DXGI_FORMAT_R32_TYPELESS,
    DXGI_FORMAT_D32_FLOAT,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R32_UINT,
    DXGI_FORMAT_R32_SINT,
    DXGI_FORMAT_R24G8_TYPELESS,
    DXGI_FORMAT_D24_UNORM_S8_UINT,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT,
    DXGI_FORMAT_R8G8_TYPELESS,
    DXGI_FORMAT_R8G8_UNORM,
    DXGI_FORMAT_R8G8_UINT,
    DXGI_FORMAT_R8G8_SNORM,
    DXGI_FORMAT_R8G8_SINT,
    DXGI_FORMAT_R16_TYPELESS,
    DXGI_FORMAT_R16_FLOAT,
    DXGI_FORMAT_D16_UNORM,
    DXGI_FORMAT_R16_UNORM,
    DXGI_FORMAT_R16_UINT,
    DXGI_FORMAT_R16_SNORM,
    DXGI_FORMAT_R16_SINT,
    DXGI_FORMAT_R8_TYPELESS,
    DXGI_FORMAT_R8_UNORM,
    DXGI_FORMAT_R8_UINT,
    DXGI_FORMAT_R8_SNORM,
    DXGI_FORMAT_R8_SINT,
    DXGI_FORMAT_A8_UNORM,
    DXGI_FORMAT_R1_UNORM,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
    DXGI_FORMAT_R8G8_B8G8_UNORM,
    DXGI_FORMAT_G8R8_G8B8_UNORM,
    DXGI_FORMAT_BC1_TYPELESS,
    DXGI_FORMAT_BC1_UNORM,
    DXGI_FORMAT_BC1_UNORM_SRGB,
    DXGI_FORMAT_BC2_TYPELESS,
    DXGI_FORMAT_BC2_UNORM,
    DXGI_FORMAT_BC2_UNORM_SRGB,
    DXGI_FORMAT_BC3_TYPELESS,
    DXGI_FORMAT_BC3_UNORM,
    DXGI_FORMAT_BC3_UNORM_SRGB,
    DXGI_FORMAT_BC4_TYPELESS,
    DXGI_FORMAT_BC4_UNORM,
    DXGI_FORMAT_BC4_SNORM,
    DXGI_FORMAT_BC5_TYPELESS,
    DXGI_FORMAT_BC5_UNORM,
    DXGI_FORMAT_BC5_SNORM,
    DXGI_FORMAT_B5G6R5_UNORM,
    DXGI_FORMAT_B5G5R5A1_UNORM,
    DXGI_FORMAT_B8G8R8A8_UNORM,
    DXGI_FORMAT_B8G8R8X8_UNORM,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
    DXGI_FORMAT_B8G8R8A8_TYPELESS,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
    DXGI_FORMAT_B8G8R8X8_TYPELESS,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB,
    DXGI_FORMAT_BC6H_TYPELESS,
    DXGI_FORMAT_BC6H_UF16,
    DXGI_FORMAT_BC6H_SF16,
    DXGI_FORMAT_BC7_TYPELESS,
    DXGI_FORMAT_BC7_UNORM,
    DXGI_FORMAT_BC7_UNORM_SRGB,
    DXGI_FORMAT_AYUV,
    DXGI_FORMAT_Y410,
    DXGI_FORMAT_Y416,
    DXGI_FORMAT_NV12,
    DXGI_FORMAT_P010,
    DXGI_FORMAT_P016,
    DXGI_FORMAT_420_OPAQUE,
    DXGI_FORMAT_YUY2,
    DXGI_FORMAT_Y210,
    DXGI_FORMAT_Y216,
    DXGI_FORMAT_NV11,
    DXGI_FORMAT_AI44,
    DXGI_FORMAT_IA44,
    DXGI_FORMAT_P8,
    DXGI_FORMAT_A8P8,
    DXGI_FORMAT_B4G4R4A4_UNORM,
    DXGI_FORMAT_P208,
    DXGI_FORMAT_V208,
    DXGI_FORMAT_V408,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE,
    DXGI_FORMAT_SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE,
    DXGI_FORMAT_FORCE_UINT
};

enum D3D10_RESOURCE_DIMENSION {
    D3D10_RESOURCE_DIMENSION_UNKNOWN,
    D3D10_RESOURCE_DIMENSION_BUFFER,
    D3D10_RESOURCE_DIMENSION_TEXTURE1D,
    D3D10_RESOURCE_DIMENSION_TEXTURE2D,
    D3D10_RESOURCE_DIMENSION_TEXTURE3D
};

struct DDS_headerDX10 {
    DXGI_FORMAT dxgiFormat;
    D3D10_RESOURCE_DIMENSION resourceDimensions;
    uint32_t miscFlags;
    uint32_t arraySize;
    uint32_t miscFlags2;
};

const uint32_t ddsMagicNumber = 0x20534444;

//see header reference, dwFlags table
namespace DDS_Flags{
    const uint32_t caps = 0x1;
    const uint32_t height = 0x2;
    const uint32_t width = 0x4;
    const uint32_t pitch = 0x8;
    const uint32_t pixelFormat = 0x1000;
    const uint32_t mipCount = 0x20000;
    const uint32_t linearSize = 0x80000;
    const uint32_t depth = 0x800000;
}

//see header reference, dwCaps
namespace DDS_Caps1 {
    const uint32_t complex = 0x8;
    const uint32_t mipmap = 0x400000;
    const uint32_t texture = 0x1000;
}

//see header reference, dwCaps2
namespace DDS_Caps2 {
    const uint32_t cubemap = 0x200;
    const uint32_t cubemapXPositive = 0x400;
    const uint32_t cubemapXNegative = 0x800;
    const uint32_t cubemapYPositive = 0x1000;
    const uint32_t cubemapYNegativecubemap = 0x2000;
    const uint32_t cubemapZPositive = 0x4000;
    const uint32_t cubemapZNegativecubemap = 0x8000;
    const uint32_t volume = 0x200000;
}

namespace DDS_pixelformatFlags {
    const uint32_t alphaPixels = 0x1;
    const uint32_t alpha = 0x2;
    const uint32_t fourCC = 0x4;
    const uint32_t rgb = 0x40;
    const uint32_t yuv = 0x200;
    const uint32_t luminance = 0x20000;
}

namespace DDS_pixelFormatCompressionCodes {
    const uint32_t DXT1 = 0x31545844;
    const uint32_t DXT2 = 0x32545844;
    const uint32_t DXT3 = 0x33545844;
    const uint32_t DXT4 = 0x34545844;
    const uint32_t DXT5 = 0x35545844;
    const uint32_t DX10 = 0x30315844;
	const uint32_t BC5	= 0x32495441;
}

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
        assert(magicNumber == ddsMagicNumber);
    }

    //read header
    DDS_header header;
    file.read((char*)&header, sizeof(header));
    
    outImage->width = header.width;
    outImage->height = header.height;
    outImage->depth = std::max(header.depth, (uint32_t)1);
    if (outImage->depth == 1) {
        if (outImage->height == 1) {
            outImage->type = ImageType::Type1D;
        }
        else {
            outImage->type = ImageType::Type2D;
        }
    }
    else {
        outImage->type = ImageType::Type3D;
    }
    
    outImage->mipCount = MipCount::Manual;
    outImage->manualMipCount = glm::max(header.mipMapCount, uint32_t(1));
    outImage->autoCreateMips = false;
    outImage->usageFlags = ImageUsageFlags::Sampled;

    bool isUsingDX10Header = false;
	const bool isCompressedFormat = header.pixelFormat.flags & DDS_pixelformatFlags::fourCC;

    //only limited formats supported at the moment
    //more formats will be added as needed    
    if (header.pixelFormat.compressionCode == DDS_pixelFormatCompressionCodes::DX10) {
        //indicates presence of DX10 header which includes image format details
        isUsingDX10Header = true;
        DDS_headerDX10 headerDX10;
        file.read((char*)&headerDX10, sizeof(DDS_headerDX10));
        if (headerDX10.dxgiFormat == DXGI_FORMAT_R16_FLOAT) {
            outImage->format = ImageFormat::R16_sFloat;
        }
        else {
            std::cout << "DDS unsupported texture format: " << filename << std::endl;
            return false;
        }
    }
    else if (header.pixelFormat.compressionCode == DDS_pixelFormatCompressionCodes::DXT1) {
        outImage->format = ImageFormat::BC1;
    }
    else if (header.pixelFormat.compressionCode == DDS_pixelFormatCompressionCodes::DXT5) {
        outImage->format = ImageFormat::BC3;
    }
	else if (header.pixelFormat.compressionCode == DDS_pixelFormatCompressionCodes::BC5) {
		outImage->format = ImageFormat::BC5;
	}
    else {
        std::cout << "DDS unsupported texture format: " << filename << std::endl;
        return false;
    }

    //data size is size of file without header and magic number
    size_t dataSize = fileSize - (sizeof(header) + sizeof(uint32_t));
    if (isUsingDX10Header) {
        dataSize -= sizeof(DDS_headerDX10);
    }

    //copy data
    outImage->initialData.resize(dataSize);
    file.read((char*)outImage->initialData.data(), dataSize);

    file.close();
    return true;
}

DDS_PixelFormat getDDSPixelFormat() {
    //legacy pixel format does not map to all needed formats
    //therefore the format is specified in the DX10 header, making this data irrelevant
    DDS_PixelFormat pixelFormat;
    pixelFormat.infoSize = sizeof(DDS_PixelFormat);
    pixelFormat.flags = 0;
    pixelFormat.RGBBitCount = 0;
    pixelFormat.compressionCode = DDS_pixelFormatCompressionCodes::DX10; //specifiy using DX10 header
    pixelFormat.RBitMask = 0;
    pixelFormat.GBitMask = 0;
    pixelFormat.BBitMask = 0;
    pixelFormat.ABitMask = 0;
    return pixelFormat;
}

void writeDDSFile(const std::filesystem::path& pathAbsolute, const ImageDescription& imageDescription) {
    //divide initialData size by four because we're going from uint8 to uint32
    const size_t binaryDataSize = sizeof(ddsMagicNumber) + sizeof(DDS_header) + sizeof(DDS_headerDX10) + imageDescription.initialData.size() / 4;
    std::vector<uint32_t> binaryData;
    binaryData.reserve(binaryDataSize);

    //magic number
    binaryData.push_back(ddsMagicNumber);

    //write header
    DDS_header header;
    {
        header.headerSize = sizeof(DDS_header);

        //flags
        header.flags = 0x0;
        //required flags
        header.flags |= DDS_Flags::caps;
        header.flags |= DDS_Flags::width;
        header.flags |= DDS_Flags::height;
        header.flags |= DDS_Flags::pixelFormat;
        if (imageDescription.mipCount != MipCount::One) {
            header.flags |= DDS_Flags::mipCount;
        }
        if (imageDescription.depth != 1) {
            header.flags |= DDS_Flags::depth;
        }

        header.height = imageDescription.height;
        header.width = imageDescription.width;

        header.pitchOrLinearSize = 0; //not supported currently, accordingly flag is not set
        header.depth = imageDescription.depth;

        if (imageDescription.mipCount == MipCount::One) {
            header.mipMapCount = 1;
        }
        else if (imageDescription.mipCount == MipCount::FullChain || imageDescription.mipCount == MipCount::FullChainAlreadyInData) {
            header.mipMapCount = mipCountFromResolution(imageDescription.width, imageDescription.height, imageDescription.depth);
        }
        else if (imageDescription.mipCount == MipCount::Manual) {
            header.mipMapCount = imageDescription.manualMipCount;
        }
        else {
            std::cout << "Error: ImageIo.cpp, writeDDSFile, unknown MipCount enum\n";
            header.mipMapCount = 1;
        }

        header.pixelFormat = getDDSPixelFormat();

        //caps
        header.caps = DDS_Caps1::texture;   //required
        if (header.mipMapCount != 1) {
            header.caps |= DDS_Caps1::mipmap;
            header.caps |= DDS_Caps1::complex;
        }
        if (imageDescription.depth != 1) {
            header.caps |= DDS_Caps1::complex;
        }

        //caps2
        if (imageDescription.depth == 1) {
            header.caps2 = 0;
        }
        else {
            header.caps2 = DDS_Caps2::volume;
        }

        //unused
        header.caps3 = 0;
        header.caps4 = 0;
        for (int i = 0; i < 11; i++) {
            header.reserved1[i] = 0;
        }
        header.reserved2 = 0;
    }    

    for (uint32_t i = 0; i < sizeof(DDS_header) / 4; i++) {
        binaryData.push_back((reinterpret_cast<uint32_t*>(&header))[i]);
    }
    
    DDS_headerDX10 headerDX10;
    if (imageDescription.format == ImageFormat::RGBA8) {
        headerDX10.dxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    else if (imageDescription.format == ImageFormat::R16_sFloat) {
        headerDX10.dxgiFormat = DXGI_FORMAT_R16_FLOAT;
    }
    else {
        throw("unsupported format");
    }
    headerDX10.arraySize = 1;
    headerDX10.miscFlags = 0;
    headerDX10.miscFlags2 = 0;
    if (imageDescription.depth == 1) {
        if (imageDescription.height == 1) {
            headerDX10.resourceDimensions = D3D10_RESOURCE_DIMENSION_TEXTURE1D;
        }
        else {
            headerDX10.resourceDimensions = D3D10_RESOURCE_DIMENSION_TEXTURE2D;
        }
    }
    else {
        headerDX10.resourceDimensions = D3D10_RESOURCE_DIMENSION_TEXTURE3D;
    }

    for (uint32_t i = 0; i < sizeof(DDS_headerDX10) / 4; i++) {
        binaryData.push_back((reinterpret_cast<uint32_t*>(&headerDX10))[i]);
    }

    for (size_t i = 0; i < imageDescription.initialData.size(); i+=4) {
        union Value {
            uint32_t dword;
            uint8_t bytes[4];
        };
        Value v;
        v.bytes[0] = imageDescription.initialData[i];
        v.bytes[1] = imageDescription.initialData[i+1];
        v.bytes[2] = imageDescription.initialData[i+2];
        v.bytes[3] = imageDescription.initialData[i+3];
        binaryData.push_back(v.dword);
    }

    writeBinaryFile(pathAbsolute, binaryData);
}