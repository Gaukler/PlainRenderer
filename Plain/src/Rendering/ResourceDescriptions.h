#pragma once
#include "pch.h"
#include "RenderHandles.h"

/*
=========
RenderPass resources
=========
*/
/*
resources are used to comunicate to a renderpass what and how a resource is used
the shader dictates what type of resource must be bound where
resources may be changed from frame to frame
*/
struct StorageBufferResource {
    StorageBufferResource(
        const StorageBufferHandle   buffer,
        const bool                  readOnly,
        const uint32_t              binding) : buffer(buffer), readOnly(readOnly), binding(binding) {};
    StorageBufferHandle buffer;
    bool                readOnly;
    uint32_t            binding;
};

struct UniformBufferResource {
    UniformBufferResource(
        const UniformBufferHandle   buffer,
        const bool                  readOnly,
        const uint32_t              binding) : buffer(buffer), readOnly(readOnly), binding(binding) {};
    UniformBufferHandle buffer;
    bool                readOnly;
    uint32_t            binding;
};

/*
if used as a storage image it is considered to be written to, causing additional barriers
*/
struct ImageResource {
    /*
    initializer list constructor with all members
    */
    ImageResource(
        const ImageHandle image,
        const uint32_t    mipLevel,
        const uint32_t    binding) : image(image), mipLevel(mipLevel), binding(binding){};
    ImageHandle     image;
    uint32_t        mipLevel;
    uint32_t        binding;
};

struct SamplerResource {
    /*
    initializer list constructor with all members
    */
    SamplerResource(
        const SamplerHandle sampler,
        const uint32_t      binding) : sampler(sampler), binding(binding){};
    SamplerHandle   sampler;
    uint32_t        binding;
};

struct RenderPassResources {
    std::vector<SamplerResource>        samplers;
    std::vector<StorageBufferResource>  storageBuffers;
    std::vector<UniformBufferResource>  uniformBuffers;
    std::vector<ImageResource>          sampledImages;
    std::vector<ImageResource>          storageImages;
};

/*
contains all info used to submit a renderpass execution to the backend
*/
struct RenderPassExecution {
    RenderPassHandle                handle;
    RenderPassResources             resources;
    std::vector<RenderPassHandle>   parents;
    uint32_t                        dispatchCount[3]; //compute pass only
};

/*
=========
RenderPass description
=========
*/

/*
config for rasterization
*/
enum class RasterizationeMode { Fill, Line, Point };
enum class CullMode { None, Front, Back };

struct RasterizationConfig {
    RasterizationeMode  mode = RasterizationeMode::Fill;
    CullMode            cullMode = CullMode::None;
};

/*
config for depth testing
*/
enum class DepthFunction { Never, Always, Less, Greater, LessEqual, GreaterEqual, Equal };

struct DepthTest {
    DepthFunction   function = DepthFunction::Always;
    bool            write = false;
};

/*
config for blending
*/
enum class BlendState { None, Additive };

/*
attachments are fixed after renderpass creation and can't be changed from frame to frame
*/
enum class AttachmentLoadOp { Load, Clear, DontCare };

struct Attachment {
    Attachment(
        const ImageHandle         image,
        const uint32_t            mipLevel,
        const uint32_t            binding,
        const AttachmentLoadOp    loadOp
    ) : image(image), mipLevel(mipLevel), binding(binding), loadOp(loadOp){};

    ImageHandle         image       = 0;
    uint32_t            mipLevel    = 0;
    uint32_t            binding     = 0;
    AttachmentLoadOp    loadOp      = AttachmentLoadOp::DontCare;
};

//only int support at the moment
struct ShaderSpecialisationConstants {
    std::vector<uint32_t>    locationIDs;
    std::vector<int>         values;
};

struct ShaderDescription {
    std::filesystem::path srcPathRelative;
    ShaderSpecialisationConstants specialisationConstants;
};

struct GraphicPassShaderDescriptions {
    ShaderDescription                 vertex;
    ShaderDescription                 fragment;
    std::optional<ShaderDescription>  geometry;
    std::optional<ShaderDescription>  tesselationControl;
    std::optional<ShaderDescription>  tesselationEvaluation;
};

struct GraphicPassDescription {
    GraphicPassShaderDescriptions   shaderDescriptions;
    std::vector<Attachment>         attachments;

    //configuration
    uint32_t                patchControlPoints; //ignored if no tesselation shader
    RasterizationConfig     rasterization;
    BlendState              blending;
    DepthTest               depthTest;
};

struct ComputePassDescription {
    ShaderDescription shaderDescription;
};

/*
=========
Image
=========
*/

enum class ImageType { Type1D, Type2D, Type3D, TypeCube };
enum class ImageFormat { R8, RG8, RGBA8, RG16_sFloat, RGBA16_sFloat, RGBA32_sFloat, R11G11B10_uFloat, Depth16, Depth32, BC1, BC3, BC5 };
enum class MipCount { One, FullChain, Manual, FullChainAlreadyInData };

typedef enum ImageUsageFlags {
    IMAGE_USAGE_STORAGE = 0x00000001,
    IMAGE_USAGE_SAMPLED = 0x00000002,
    IMAGE_USAGE_ATTACHMENT = 0x00000004,
} ImageUsageFlags;


struct ImageDescription {
    /*
    default constructor
    */
    ImageDescription() {};
    /*
    initializer list constructor with all members
    */
    ImageDescription(
        const std::vector<char> initialData,
        const uint32_t          width,
        const uint32_t          height,
        const uint32_t          depth,
        const ImageType         type,
        const ImageFormat       format,
        const ImageUsageFlags   usageFlags,
        const MipCount          mipCount,
        const uint32_t          manualMipCount,
        const bool              autoCreateMips) : 
        width(width), height(height), depth(depth), type(type), format(format), usageFlags(usageFlags), mipCount(mipCount), 
        manualMipCount(manualMipCount), autoCreateMips(autoCreateMips){};

    std::vector<char> initialData;

    uint32_t width  = 1;
    uint32_t height = 0;
    uint32_t depth  = 0;

    ImageType       type    = ImageType::Type1D;
    ImageFormat     format  = ImageFormat::R8;
    ImageUsageFlags usageFlags = (ImageUsageFlags)0;

    MipCount    mipCount        = MipCount::One;
    uint32_t    manualMipCount  = 1; //only used if mipCount is Manual
    bool        autoCreateMips  = false;
};

/*
=========
Buffer
=========
*/

enum class BufferType { Storage, Uniform };

struct BufferDescription {
    BufferType  type;
    size_t      size;
    void*       initialData = nullptr;
};

/*
=========
Sampler
=========
*/

enum class SamplerInterpolation { Nearest, Linear };
enum class SamplerWrapping { Clamp, Color, Repeat };
enum class SamplerBorderColor { White, Black };

struct SamplerDescription {
    /*
    default constructor
    */
    SamplerDescription() {};
    /*
    initializer list constructor with all members
    */
    SamplerDescription(
    const SamplerInterpolation    interpolation,
    const SamplerWrapping         wrapping,
    const bool                    useAnisotropy,
    const float                   maxAnisotropy,
    const SamplerBorderColor      borderColor,
    const uint32_t                maxMip
    ) : interpolation(interpolation), wrapping(wrapping), useAnisotropy(useAnisotropy),
        maxAnisotropy(maxAnisotropy), borderColor(borderColor), maxMip(maxMip) {};

    SamplerInterpolation    interpolation   = SamplerInterpolation::Nearest;
    SamplerWrapping         wrapping        = SamplerWrapping::Repeat;
    bool                    useAnisotropy   = false;
    float                   maxAnisotropy   = 8;                            //only used if useAnisotropy is true
    SamplerBorderColor      borderColor     = SamplerBorderColor::Black;    //only used if wrapping is Color
    uint32_t                maxMip          = 1;
};

/*
=========
Global shader info
=========
*/

struct GlobalShaderInfo {
    glm::vec4 sunColor = glm::vec4(1.f);
    glm::vec4 sunDirection = glm::vec4(0.f, -1.f, 0.f, 0.f);
    glm::mat4 lightMatrix = glm::mat4(1.f);
    glm::vec4 cameraPos = glm::vec4(0.f);
    float sunIlluminanceLux = 80000.f;
    float skyIlluminanceLux = 30000.f;
    float exposureOffset = 0.f;
    float exposureAdaptionSpeedEvPerSec = 2.f;
    float delteTime = 0.016f;
};