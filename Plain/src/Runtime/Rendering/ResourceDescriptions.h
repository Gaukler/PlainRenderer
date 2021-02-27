#pragma once
#include "pch.h"
#include "RenderHandles.h"
#include "ImageDescription.h"

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


//generic info for a renderpass
struct RenderPassExecution {
    RenderPassHandle                handle;
    RenderPassResources             resources;
    std::vector<RenderPassHandle>   parents;
};

//contains RenderPassExecution and additional info for graphic pass
struct GraphicPassExecution {
	RenderPassExecution genericInfo;
	FramebufferHandle framebuffer;
};

//contains RenderPassExecution and additional info for compute pass
struct ComputePassExecution {
	RenderPassExecution genericInfo;
	std::vector<char> pushConstants;
	uint32_t dispatchCount[3] = { 1, 1, 1 };
};

enum class RasterizationeMode { Fill, Line, Point };
enum class CullMode { None, Front, Back };

struct RasterizationConfig {
    RasterizationeMode  mode = RasterizationeMode::Fill;
    CullMode            cullMode = CullMode::None;
    bool                clampDepth = false;
	bool				conservative = false;
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
        const ImageFormat       format,
        const AttachmentLoadOp  loadOp
    ) : format(format), loadOp(loadOp){};

    ImageFormat         format  = ImageFormat::R8;
    AttachmentLoadOp    loadOp  = AttachmentLoadOp::DontCare;
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

    //configuration
    std::vector<Attachment> attachments;
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
	glm::mat4 viewProjection = glm::mat4(0.f);
    glm::vec4 sunDirection  = glm::vec4(0.f, -1.f, 0.f, 0.f);
    glm::vec4 cameraPos     = glm::vec4(0.f);
    glm::vec4 cameraRight   = glm::vec4(1.f, 0.f, 0.f, 0.f);
    glm::vec4 cameraUp      = glm::vec4(0.f, -1.f, 0.f, 0.f);
    glm::vec4 cameraForward = glm::vec4(0.f, 0.f, -1.f, 0.f);
    glm::vec2 currentFrameCameraJitter  = glm::vec2(0.f);
    glm::vec2 previousFrameCameraJitter = glm::vec2(0.f);
    glm::ivec2 screenResolution = glm::ivec2(0);
    float cameraTanFovHalf = 1.f;
    float cameraAspectRatio = 1.f;
    float nearPlane = 0.1f;
    float farPlane = 100.f;
    float sunIlluminanceLux = 128000.f; //from: https://en.wikipedia.org/wiki/Sunlight
    float exposureOffset = 1.f;
    float exposureAdaptionSpeedEvPerSec = 2.f;
    float deltaTime = 0.016f;
    float time = 0.f;
    float mipBias = 0.f;
    bool cameraCut = false;
	uint32_t frameIndex = 0;
};

struct FramebufferTarget {
    ImageHandle image;
    uint32_t mipLevel = 0;
};

struct FramebufferDescription {
    std::vector<FramebufferTarget> targets;
    RenderPassHandle compatibleRenderpass; //framebuffer can be used with this pass or compatible ones
};