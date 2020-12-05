#pragma once
#include "pch.h"
#include "Runtime/Rendering/RenderHandles.h"
#include "Runtime/Rendering/ResourceDescriptions.h"
#include "Runtime/Rendering/Backend/Resources.h"

namespace fs = std::filesystem;

struct GraphicPassShaderReloadInfo {
    RenderPassHandle renderpass;
    GraphicPassShaderSpirV spirV;
};

struct ComputePassShaderReloadInfo {
    RenderPassHandle renderpass;
    std::vector<uint32_t> spirV;
};

struct ShaderLoadInfo {
    //indices index into m_filePaths and m_fileLastChanges
    size_t shaderFileIndex;
    size_t spirvCacheFileIndex;
};

struct GraphicShadersLoadInfo {
    ShaderLoadInfo vertex;
    ShaderLoadInfo fragment;
    std::optional<ShaderLoadInfo> geometry;
    std::optional<ShaderLoadInfo> tessellationControl;
    std::optional<ShaderLoadInfo> tessellationEvaluation;
};

struct ComputeShaderSourceInfo {
    RenderPassHandle renderpass;    //stored to efficiently connect out of date shader list to corresponding passes
    ShaderLoadInfo loadInfo;
    std::unordered_set<size_t> includeFileIndices;
};

struct GraphicShaderSourceInfo {
    RenderPassHandle renderpass;    //stored to efficiently connect out of date shader list to corresponding passes
    GraphicShadersLoadInfo loadInfo;
    std::unordered_set<size_t> includeFileIndices;
};

//manages shader loading and hot reloading
//uses spirv cache files to reuse compiled results between app runs
//tracks include files to detect if a shader needs to be reloaded due to include file change
class ShaderFileManager {
public:    
    ComputeShaderHandle addComputeShader(const ShaderDescription& computeShaderDesc);
    GraphicShadersHandle addGraphicShaders(const GraphicPassShaderDescriptions& graphicShadersDesc);

    //add a renderpass handle that is returned when a render pass is out of date
    void setGraphicPassHandle(const GraphicShadersHandle shaderHandle, const RenderPassHandle passHandle);

    //add compute pass handle that is returned when shader is out of date
    void setComputePassHandle(const ComputeShaderHandle shaderHandle, const RenderPassHandle passHandle);
    
    bool loadComputeShaderSpirV(const ComputeShaderHandle handle, std::vector<uint32_t>* outSpirV);
    bool loadGraphicShadersSpirV(const GraphicShadersHandle handle, GraphicPassShaderSpirV* outSpirV);

    //iterates over all shader files and updates the last changed time point
    //must be called before reloading out of date shaders so that out of date check is correct
    void updateFileLastChangeTimes();

    std::vector<ComputePassShaderReloadInfo> reloadOutOfDateComputeShaders();
    std::vector<GraphicPassShaderReloadInfo> reloadOutOfDateGraphicShaders();
private:
    //returns existing index if available
    //else adds entry in m_pathToFileIndex, m_filePathsAbsolute and m_fileLastChanges
    size_t addFilePath(const fs::path& filePathAbsolute);
    
    bool isComputeShaderCacheOutOfDate(const ComputeShaderSourceInfo& shaderSrcInfo);
    bool areGraphicShadersCachesOutOfDate(const GraphicShaderSourceInfo& shaderSrcInfo);

    ShaderLoadInfo loadInfoFromShaderDescription(const ShaderDescription& shaderDesc);
    //loads GLSL shader file, parses include paths, then adds them to index set
    //outIndexSet must not be nullptr
    void addGLSLIncludesFileIndicesToSet(const fs::path& shaderPathRelative, std::unordered_set<size_t>* outIndexSet);

    //---- source information ----

    std::vector<ComputeShaderSourceInfo> m_computeShaderSourceInfos;
    std::vector<GraphicShaderSourceInfo> m_graphicShaderSourceInfos;

    //---- file information ----

    //map to speed up search for already existing files
    //using string instead of path to use default string hash
    std::unordered_map<std::string, size_t> m_pathToFileIndex;

    std::vector<fs::path> m_filePathsAbsolute;
    std::vector<fs::file_time_type> m_fileLastChanges;
};