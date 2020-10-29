#pragma once
#include "pch.h"
#include "RenderHandles.h"


//resources are used to comunicate to a renderpass what and how a resource is used
//the shader dictates what type of resource must be bound where
//resources may be changed from frame to frame
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
        const uint32_t              binding) : buffer(buffer), binding(binding) {};
    UniformBufferHandle buffer;
    uint32_t            binding;
};


//if used as a storage image it is considered to be written to, causing additional barriers
struct ImageResource {
    ImageResource(
        const ImageHandle image,
        const uint32_t    mipLevel,
        const uint32_t    binding) : image(image), mipLevel(mipLevel), binding(binding){};
    ImageHandle     image;
    uint32_t        mipLevel;
    uint32_t        binding;
};

struct SamplerResource {
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


//contains all info used to submit a renderpass execution to the backend
struct RenderPassExecution {
    RenderPassHandle                handle;
    RenderPassResources             resources;
    std::vector<RenderPassHandle>   parents;
    uint32_t                        dispatchCount[3] = { 1, 1, 1}; //compute pass only
};


enum class RasterizationeMode { Fill, Line, Point };
enum class CullMode { None, Front, Back };

struct RasterizationConfig {
    RasterizationeMode  mode = RasterizationeMode::Fill;
    CullMode            cullMode = CullMode::None;
    bool                clampDepth = false;
};


enum class DepthFunction { Never, Always, Less, Greater, LessEqual, GreaterEqual, Equal };

struct DepthTest {
    DepthFunction   function = DepthFunction::Always;
    bool            write = false;
};


enum class BlendState { None, Additive };
enum class AttachmentLoadOp { Load, Clear, DontCare };

struct Attachment {
    Attachment(
        const ImageHandle         image,
        const uint32_t            mipLevel,
        const uint32_t            binding,
        const AttachmentLoadOp    loadOp
    ) : image(image), mipLevel(mipLevel), binding(binding), loadOp(loadOp){};

    ImageHandle         image;
    uint32_t            mipLevel    = 0;
    uint32_t            binding     = 0;
    AttachmentLoadOp    loadOp      = AttachmentLoadOp::DontCare;
};

//data will be interpreted according to shader
struct SpecialisationConstant {
    uint32_t location;
    std::vector<char> data;
};

struct ShaderDescription {
    std::filesystem::path srcPathRelative;
    std::vector<SpecialisationConstant> specialisationConstants;
};

struct GraphicPassShaderDescriptions {
    ShaderDescription                 vertex;
    ShaderDescription                 fragment;
    std::optional<ShaderDescription>  geometry;
    std::optional<ShaderDescription>  tesselationControl;
    std::optional<ShaderDescription>  tesselationEvaluation;
};

enum class VertexFormat { Full, PositionOnly };

struct GraphicPassDescription {
    GraphicPassShaderDescriptions   shaderDescriptions;
    std::vector<Attachment>         attachments;

    //configuration
    uint32_t                patchControlPoints = 0; //ignored if no tesselation shader
    RasterizationConfig     rasterization;
    BlendState              blending = BlendState::None;
    DepthTest               depthTest;
    VertexFormat            vertexFormat = VertexFormat::Full;

    std::string name; //used for debug labels
};

struct ComputePassDescription {
    ShaderDescription shaderDescription;
    std::string name; //used for debug labels
};

enum class ImageType { Type1D, Type2D, Type3D, TypeCube };
enum class ImageFormat { R8, RG8, RGBA8, RG16_sFloat, RG32_sFloat, RGBA16_sFloat, RGBA16_sNorm, RGBA32_sFloat, R11G11B10_uFloat, Depth16, Depth32, BC1, BC3, BC5 };
enum class MipCount { One, FullChain, Manual, FullChainAlreadyInData };

enum class ImageUsageFlags : uint32_t {
    Storage     = 0x00000001,
    Sampled     = 0x00000002,
    Attachment  = 0x00000004,
};

ImageUsageFlags operator&(const ImageUsageFlags l, const ImageUsageFlags r);
ImageUsageFlags operator|(const ImageUsageFlags l, const ImageUsageFlags r);


struct ImageDescription {
    std::vector<uint8_t> initialData;

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

struct UniformBufferDescription {
    size_t  size = 0;
    void*   initialData = nullptr;
};

struct StorageBufferDescription {
    size_t  size = 0;
    void*   initialData = nullptr;
};

enum class SamplerInterpolation { Nearest, Linear };
enum class SamplerWrapping { Clamp, Color, Repeat };
enum class SamplerBorderColor { White, Black };

struct SamplerDescription {
    SamplerInterpolation    interpolation   = SamplerInterpolation::Nearest;
    SamplerWrapping         wrapping        = SamplerWrapping::Repeat;
    bool                    useAnisotropy   = false;
    float                   maxAnisotropy   = 8;                            //only used if useAnisotropy is true
    SamplerBorderColor      borderColor     = SamplerBorderColor::Black;    //only used if wrapping is Color
    uint32_t                maxMip          = 1;
};

struct GlobalShaderInfo {
    glm::vec4 sunDirection  = glm::vec4(0.f, -1.f, 0.f, 0.f);
    glm::vec4 cameraPos     = glm::vec4(0.f);
    glm::vec4 cameraRight   = glm::vec4(1.f, 0.f, 0.f, 0.f);
    glm::vec4 cameraUp      = glm::vec4(0.f, -1.f, 0.f, 0.f);
    glm::vec4 cameraForward = glm::vec4(0.f, 0.f, -1.f, 0.f);
    glm::vec2 currentFrameCameraJitter  = glm::vec2(0.f);
    glm::vec2 previousFrameCameraJitter = glm::vec2(0.f);
    float cameraTanFovHalf = 1.f;
    float cameraAspectRatio = 1.f;
    float nearPlane = 0.1f;
    float farPlane = 100.f;
    float sunIlluminanceLux = 128000.f; //from: https://en.wikipedia.org/wiki/Sunlight
    float exposureOffset = 1.f;
    float exposureAdaptionSpeedEvPerSec = 2.f;
    float deltaTime = 0.016f;
    float time = 0.f;
};

struct Material {
    ImageHandle diffuseTexture;
    ImageHandle normalTexture;
    ImageHandle specularTexture;
};