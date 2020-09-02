#include "pch.h"
#include "ShaderCompile.h"
#include "Utilities/DirectoryUtils.h"
#include "ShaderIO.h"

//disable glslang warnings
#pragma warning( push )
#pragma warning( disable : 26495 26812 26451)

#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <StandAlone/DirStackFileIncluder.h>

//reenable warnings
#pragma warning( pop )

//from StandAlone/ResourceLimits.cpp
const TBuiltInResource DefaultTBuiltInResource = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .maxMeshOutputVerticesNV = */ 256,
    /* .maxMeshOutputPrimitivesNV = */ 512,
    /* .maxMeshWorkGroupSizeX_NV = */ 32,
    /* .maxMeshWorkGroupSizeY_NV = */ 1,
    /* .maxMeshWorkGroupSizeZ_NV = */ 1,
    /* .maxTaskWorkGroupSizeX_NV = */ 32,
    /* .maxTaskWorkGroupSizeY_NV = */ 1,
    /* .maxTaskWorkGroupSizeZ_NV = */ 1,
    /* .maxMeshViewCountNV = */ 4,
    /* .maxDualSourceDrawBuffersEXT = */ 1,

    /* .limits = */ {
        /* .nonInductiveForLoops = */ 1,
        /* .whileLoops = */ 1,
        /* .doWhileLoops = */ 1,
        /* .generalUniformIndexing = */ 1,
        /* .generalAttributeMatrixVectorIndexing = */ 1,
        /* .generalVaryingIndexing = */ 1,
        /* .generalSamplerIndexing = */ 1,
        /* .generalVariableIndexing = */ 1,
        /* .generalConstantMatrixVectorIndexing = */ 1,
    } };


static bool glslangInitialized = false;

bool compileGLSLToSPIRV(const std::vector<char>& code, const std::filesystem::path& absolutePath, std::vector<uint32_t>* outSpirV) {

    if (!glslangInitialized) {
        glslang::InitializeProcess();
        glslangInitialized = true;
    }

    const auto extension = absolutePath.extension();
    EShLanguage shaderType;
    if (extension == ".vert") {
        shaderType = EShLangVertex;
    }
    else if (extension == ".frag") {
        shaderType = EShLangFragment;
    }
    else if (extension == ".geom") {
        shaderType = EShLangGeometry;
    }
    else if (extension == ".tese") {
        shaderType = EShLangTessEvaluation;
    }
    else if (extension == ".tesc") {
        shaderType = EShLangTessControl;
    }
    else if (extension == ".comp") {
        shaderType = EShLangCompute;
    }
    else {
        std::cout << "Unknown shader extension: " << absolutePath << "\n";
        shaderType = EShLangCount;
    }

    glslang::TShader shader(shaderType);
    const char* codeCString = code.data();
    shader.setStrings(&codeCString, 1);

    const int shaderVersion = 100;
    shader.setEnvInput(glslang::EShSourceGlsl, shaderType, glslang::EShClientVulkan, shaderVersion);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

    const TBuiltInResource resourceLimits = DefaultTBuiltInResource;
    const int defaultVersion = 100;

    EShMessages messageFlags = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
    std::string preprocessedGLSL;

    DirStackFileIncluder includer;
    includer.pushExternalLocalDirectory(absolutePath.parent_path().string());
    
    if (!shader.preprocess(&resourceLimits, defaultVersion, ENoProfile, false, false, messageFlags, &preprocessedGLSL, includer)) {
        std::cout << "GLSL preprocessing failed: " << absolutePath << std::endl;
        std::cout << shader.getInfoLog();
        std::cout << shader.getInfoDebugLog();
        return false;
    }

    const char* preprocessedCString = preprocessedGLSL.c_str();
    shader.setStrings(&preprocessedCString, 1);

    if (!shader.parse(&resourceLimits, defaultVersion, false, messageFlags)) {
        std::cout << "GLSL Parsing failed: " << absolutePath << std::endl;
        std::cout << shader.getInfoLog();
        std::cout << shader.getInfoDebugLog();
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(messageFlags)) {
        std::cout << "Shader linking failed: " << absolutePath << std::endl;
        std::cout << program.getInfoLog();
        std::cout << program.getInfoDebugLog();
        return false;
    }

    spv::SpvBuildLogger logger;
    glslang::SpvOptions spvOptions;
    glslang::GlslangToSpv(*program.getIntermediate(shaderType), *outSpirV, &logger, &spvOptions);

    return true;
}