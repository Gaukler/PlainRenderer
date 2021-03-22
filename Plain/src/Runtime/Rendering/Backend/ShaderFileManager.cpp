#include "pch.h"
#include "ShaderFileManager.h"
#include "Common/FileIO.h"
#include "ShaderIO.h"
#include "Common/Utilities/DirectoryUtils.h"

#define NOMINMAX
#include <Windows.h>

//not terribly robust, e.g. not checking if include is commented out, but it does it's job
std::vector<fs::path> parseIncludePathsFromGLSL(const std::vector<char>& glslCode) {
    std::string codeString = std::string(glslCode.data());
    std::vector<fs::path> includes;

    size_t searchPos = 0;
    while (true){
        searchPos = codeString.find("#include", searchPos);
        if (searchPos != std::string::npos) {
            const size_t includeStart = codeString.find("\"", searchPos) + 1; //+1 to skip leading "
            const size_t includeEnd = codeString.find("\"", includeStart);
            if (includeStart == std::string::npos || includeEnd == std::string::npos) {
                std::cout << "Error: could not find end of shader include\n";
            }
            const std::string includeName = codeString.substr(includeStart, includeEnd - includeStart);
            const std::string absoluteIncludePath = relativeShaderPathToAbsolute(includeName).string();
            includes.push_back(absoluteIncludePath);
            searchPos = includeEnd; //continue searching where ended
        }
        else {
            break;
        }
    } 
    return includes;
}

void ShaderFileManager::setup() {
	m_isRunning = true;
	updateFileLastChangeTimes();
	m_directoryWatcher = std::thread([this]() {
		fileWatcherThread();
	});
}

void ShaderFileManager::shutdown() {
	m_isRunning = false;
	m_directoryWatcher.join();
}

void ShaderFileManager::fileWatcherThread() {
	//using windows api to report file change details was inconsistent
	//because of this the windows api is now only used to inform about a change in the directory
	//the details of which files are out of date are handled manually
	//TODO: fix callback triggers when writing spirv-cache files, causing unnecessary file out-of-date checks
	const fs::path shaderPath = DirectoryUtils::getResourceDirectory() / getShaderDirectory();
	HANDLE watchHandle = FindFirstChangeNotificationW((WCHAR*)shaderPath.c_str(), true, FILE_NOTIFY_CHANGE_LAST_WRITE);
	assert(INVALID_HANDLE_VALUE);

	const DWORD waitInMs = 10;
	while (m_isRunning) {

		const DWORD waitResult = WaitForSingleObject(watchHandle, waitInMs);
		FindNextChangeNotification(watchHandle);
		if (waitResult == WAIT_OBJECT_0) {
			//event occured
			//writing can still be in progress when watchHandle is signaled
			//FIXME: robust solution
			Sleep(1);
			updateFileLastChangeTimes();

			m_outOfDateListsMutex.lock();
			for (int i = 0; i < m_computeShaderSourceInfos.size(); i++) {
				if (isComputeShaderCacheOutOfDate(m_computeShaderSourceInfos[i])) {
					m_outOfDateComputeIndices.push_back(i);
				}
			}
			for (int i = 0; i < m_graphicShaderSourceInfos.size(); i++) {
				if (areGraphicShadersCachesOutOfDate(m_graphicShaderSourceInfos[i])) {
					m_outOfDateGraphicIndices.push_back(i);
				}
			}

			m_outOfDateListsMutex.unlock();
		}
		else if(waitResult == WAIT_TIMEOUT){
			//repeat loop			
		}
		else if (waitResult == WAIT_FAILED) {
			std::cout << "ShaderFileManager::fileWatcherThread Error: " << GetLastError() << "\n";
		}
	}
}

ComputeShaderHandle ShaderFileManager::addComputeShader(const ShaderDescription& computeShaderDesc) {
    ComputeShaderSourceInfo srcInfo;
    srcInfo.loadInfo = loadInfoFromShaderDescription(computeShaderDesc);
    const fs::path shaderPathAbsolute = m_filePathsAbsolute[srcInfo.loadInfo.shaderFileIndex];
    addGLSLIncludesFileIndicesToSet(shaderPathAbsolute, &srcInfo.includeFileIndices);

    //save info and return handle
    ComputeShaderHandle shaderHandle = { uint32_t(m_computeShaderSourceInfos.size()) };
    m_computeShaderSourceInfos.push_back(srcInfo);
    return shaderHandle;
}

GraphicShadersHandle ShaderFileManager::addGraphicShaders(const GraphicPassShaderDescriptions& graphicShadersDesc) {
    GraphicShaderSourceInfo srcInfo;
    //vertex
    {
        srcInfo.loadInfo.vertex = loadInfoFromShaderDescription(graphicShadersDesc.vertex);
        const fs::path path = m_filePathsAbsolute[srcInfo.loadInfo.vertex.shaderFileIndex];
        addGLSLIncludesFileIndicesToSet(path, &srcInfo.includeFileIndices);
    }
    //fragment
    {
        srcInfo.loadInfo.fragment = loadInfoFromShaderDescription(graphicShadersDesc.fragment);
        const fs::path path = m_filePathsAbsolute[srcInfo.loadInfo.fragment.shaderFileIndex];
        addGLSLIncludesFileIndicesToSet(path, &srcInfo.includeFileIndices);
    }
    //geometry
    if (graphicShadersDesc.geometry.has_value()) {
        srcInfo.loadInfo.geometry = loadInfoFromShaderDescription(graphicShadersDesc.geometry.value());
        const fs::path path = m_filePathsAbsolute[srcInfo.loadInfo.geometry.value().shaderFileIndex];
        addGLSLIncludesFileIndicesToSet(path, &srcInfo.includeFileIndices);
    }
    //tesselation control
    if (graphicShadersDesc.tesselationControl.has_value()) {
        srcInfo.loadInfo.tessellationControl = loadInfoFromShaderDescription(graphicShadersDesc.tesselationControl.value());
        const fs::path path = m_filePathsAbsolute[srcInfo.loadInfo.tessellationControl.value().shaderFileIndex];
        addGLSLIncludesFileIndicesToSet(path, &srcInfo.includeFileIndices);
    }
    //tesselation evaluation
    if (graphicShadersDesc.tesselationEvaluation.has_value()) {
        srcInfo.loadInfo.tessellationEvaluation = loadInfoFromShaderDescription(graphicShadersDesc.tesselationEvaluation.value());
        const fs::path path = m_filePathsAbsolute[srcInfo.loadInfo.tessellationEvaluation.value().shaderFileIndex];
        addGLSLIncludesFileIndicesToSet(path, &srcInfo.includeFileIndices);
    }

    //save info and return handle
    GraphicShadersHandle shaderHandle = { uint32_t(m_graphicShaderSourceInfos.size()) };
    m_graphicShaderSourceInfos.push_back(srcInfo);
    return shaderHandle;
}

void ShaderFileManager::setGraphicPassHandle(const GraphicShadersHandle shaderHandle, const RenderPassHandle passHandle) {
    m_graphicShaderSourceInfos[shaderHandle.index].renderpass = passHandle;
}

void ShaderFileManager::setComputePassHandle(const ComputeShaderHandle shaderHandle, const RenderPassHandle passHandle) {
    m_computeShaderSourceInfos[shaderHandle.index].renderpass = passHandle;
}

bool ShaderFileManager::loadComputeShaderSpirV(const ComputeShaderHandle handle, std::vector<uint32_t>* outSpirV) const {
    const ComputeShaderSourceInfo srcInfo = m_computeShaderSourceInfos[handle.index];
    if (isComputeShaderCacheOutOfDate(srcInfo)) {
        const fs::path filePath = m_filePathsAbsolute[srcInfo.loadInfo.shaderFileIndex];
        std::cout << "SpirV cache out of date, reloading: " << filePath << "\n";
        return loadShader(filePath, outSpirV);
    }
    else {
        const fs::path cachePath = m_filePathsAbsolute[srcInfo.loadInfo.spirvCacheFileIndex];
        return loadBinaryFile(cachePath, outSpirV);
    }
}

bool ShaderFileManager::loadGraphicShadersSpirV(const GraphicShadersHandle handle, GraphicPassShaderSpirV* outSpirV) const {
    const GraphicShaderSourceInfo srcInfo = m_graphicShaderSourceInfos[handle.index];
    if (areGraphicShadersCachesOutOfDate(srcInfo)) {
        //load GLSL and compile
        GraphicPassShaderPaths paths;
        //vertex and fragment paths
        paths.vertex = m_filePathsAbsolute[srcInfo.loadInfo.vertex.shaderFileIndex];
        paths.fragment = m_filePathsAbsolute[srcInfo.loadInfo.fragment.shaderFileIndex];

        std::cout << "SpirV cache out of date, reloading:\n";
        std::cout << paths.vertex << "\n";
        std::cout << paths.fragment << "\n";

        //geometry path
        if (srcInfo.loadInfo.geometry.has_value()) {
            paths.geometry = m_filePathsAbsolute[srcInfo.loadInfo.geometry.value().shaderFileIndex];
            std::cout << paths.geometry.value() << "\n";
        }
        //tessellation control path
        if (srcInfo.loadInfo.tessellationControl.has_value()) {
            paths.tessellationControl = m_filePathsAbsolute[srcInfo.loadInfo.tessellationControl.value().shaderFileIndex];
            std::cout << paths.tessellationControl.value() << "\n";
        }
        //tessellation evaluation path
        if (srcInfo.loadInfo.tessellationEvaluation.has_value()) {
            paths.tessellationEvaluation = m_filePathsAbsolute[srcInfo.loadInfo.tessellationEvaluation.value().shaderFileIndex];
            std::cout << paths.tessellationEvaluation.value() << "\n";
        }
        std::cout << "\n";

        return loadGraphicPassShaders(paths, outSpirV);
    }
    else {
        //load binary caches
        bool success = true;
        //vertex
        {
            const fs::path path = m_filePathsAbsolute[srcInfo.loadInfo.vertex.spirvCacheFileIndex];
            success &= loadBinaryFile(path, &outSpirV->vertex);
        }
        //fragment
        {
            const fs::path path = m_filePathsAbsolute[srcInfo.loadInfo.fragment.spirvCacheFileIndex];
            success &= loadBinaryFile(path, &outSpirV->fragment);
        }
        //geometry
        if (srcInfo.loadInfo.geometry.has_value()) {
            const fs::path path = m_filePathsAbsolute[srcInfo.loadInfo.geometry.value().spirvCacheFileIndex];
			outSpirV->geometry = std::vector<uint32_t>();
            success &= loadBinaryFile(path, &outSpirV->geometry.value());
        }
        //tessellation evaluation
        if (srcInfo.loadInfo.tessellationControl.has_value()) {
            const fs::path path = m_filePathsAbsolute[srcInfo.loadInfo.tessellationControl.value().spirvCacheFileIndex];
			outSpirV->tessellationControl = std::vector<uint32_t>();
            success &= loadBinaryFile(path, &outSpirV->tessellationControl.value());
        }
        //geometry
        if (srcInfo.loadInfo.tessellationEvaluation.has_value()) {
            const fs::path path = m_filePathsAbsolute[srcInfo.loadInfo.tessellationEvaluation.value().spirvCacheFileIndex];
			outSpirV->tessellationEvaluation = std::vector<uint32_t>();
            success &= loadBinaryFile(path, &outSpirV->tessellationEvaluation.value());
        }
        return success;
    }
}

std::vector<ComputePassShaderReloadInfo> ShaderFileManager::reloadOutOfDateComputeShaders() {
    std::vector<ComputePassShaderReloadInfo> reloadList;
	std::set<size_t> spirvCacheUpdateList;

	m_outOfDateListsMutex.lock();
	for (const int passIndex : m_outOfDateComputeIndices) {

		ComputeShaderSourceInfo& shaderSrcInfo = m_computeShaderSourceInfos[passIndex];
        //reload glsl and compile to spirv
        fs::path shaderPathAbsolute = m_filePathsAbsolute[shaderSrcInfo.loadInfo.shaderFileIndex];
        std::cout << "Shader out of date reloading: " << shaderPathAbsolute << "\n";

        ComputePassShaderReloadInfo reloadInfo;
        reloadInfo.renderpass = shaderSrcInfo.renderpass;
        if (loadShader(shaderPathAbsolute, &reloadInfo.spirV)) {
            reloadList.push_back(reloadInfo);

            //update spirv cache
            const fs::path spirvCachePathAbsolute = m_filePathsAbsolute[shaderSrcInfo.loadInfo.spirvCacheFileIndex];
            writeBinaryFile(spirvCachePathAbsolute, reloadInfo.spirV);

            //rebuild include file list to accomodate changes
            shaderSrcInfo.includeFileIndices.clear();
            addGLSLIncludesFileIndicesToSet(shaderPathAbsolute, &shaderSrcInfo.includeFileIndices);
        }
		//update latest cache update point
		//even if load/compilation is not succesfull we don't want to keep failing every frame before problem is solved
		//update is deferred because the same cache might be used multiple shaders
		spirvCacheUpdateList.insert(shaderSrcInfo.loadInfo.spirvCacheFileIndex);          
    }
	m_outOfDateComputeIndices.clear();
	m_outOfDateListsMutex.unlock();

	//deferred update of cache files last change times
	for (const size_t index : spirvCacheUpdateList) {
		m_fileLastChanges[index] = fs::_File_time_clock::now();
	}
	
    return reloadList;
}

std::vector<GraphicPassShaderReloadInfo> ShaderFileManager::reloadOutOfDateGraphicShaders() {

    std::vector<GraphicPassShaderReloadInfo> reloadList;
	m_outOfDateListsMutex.lock();
    for (const int passIndex : m_outOfDateGraphicIndices) {

		GraphicShaderSourceInfo& shaderSrcInfo = m_graphicShaderSourceInfos[passIndex];

        const bool hasGeometryShader = shaderSrcInfo.loadInfo.geometry.has_value();
        const bool hasTessellationControlShader = shaderSrcInfo.loadInfo.tessellationControl.has_value();
        const bool hasTessellationEvauluationShader = shaderSrcInfo.loadInfo.tessellationEvaluation.has_value();

        //reload glsl and compile to spirv
        GraphicPassShaderReloadInfo reloadInfo;
        reloadInfo.renderpass = shaderSrcInfo.renderpass;

        GraphicPassShaderPaths shaderPaths;
        shaderPaths.vertex = m_filePathsAbsolute[shaderSrcInfo.loadInfo.vertex.shaderFileIndex];
        shaderPaths.fragment = m_filePathsAbsolute[shaderSrcInfo.loadInfo.fragment.shaderFileIndex];

        std::cout << "Graphic shaders out of date reloading:\n";
        std::cout << shaderPaths.vertex << "\n";
        std::cout << shaderPaths.fragment << "\n";

        if (hasGeometryShader) {
            shaderPaths.geometry = m_filePathsAbsolute[shaderSrcInfo.loadInfo.geometry.value().shaderFileIndex];
            std::cout << shaderPaths.geometry.value() << "\n";
        }
        if (hasTessellationControlShader) {
            shaderPaths.tessellationControl = m_filePathsAbsolute[shaderSrcInfo.loadInfo.tessellationControl.value().shaderFileIndex];
            std::cout << shaderPaths.tessellationControl.value() << "\n";
        }
        if (hasTessellationEvauluationShader) {
            shaderPaths.tessellationEvaluation = m_filePathsAbsolute[shaderSrcInfo.loadInfo.tessellationEvaluation.value().shaderFileIndex];
            std::cout << shaderPaths.tessellationEvaluation.value() << "\n";
        }
        std::cout << "\n";

        if (loadGraphicPassShaders(shaderPaths, &reloadInfo.spirV)) {
            reloadList.push_back(reloadInfo);

            //update spirv caches and include file list
            shaderSrcInfo.includeFileIndices.clear();
            //vertex
            {
                const fs::path vertexCachePathAbsolute = m_filePathsAbsolute[shaderSrcInfo.loadInfo.vertex.spirvCacheFileIndex];
                writeBinaryFile(vertexCachePathAbsolute, reloadInfo.spirV.vertex);
                addGLSLIncludesFileIndicesToSet(shaderPaths.vertex, &shaderSrcInfo.includeFileIndices);
            }
            //fragment
            {
                const fs::path fragmentCachePathAbsolute = m_filePathsAbsolute[shaderSrcInfo.loadInfo.fragment.spirvCacheFileIndex];
                writeBinaryFile(fragmentCachePathAbsolute, reloadInfo.spirV.fragment);
                addGLSLIncludesFileIndicesToSet(shaderPaths.fragment, &shaderSrcInfo.includeFileIndices);
            }
            //geometry
            if (hasGeometryShader) {
                const fs::path geometryCachePathAbsolute = m_filePathsAbsolute[shaderSrcInfo.loadInfo.geometry.value().spirvCacheFileIndex];
                writeBinaryFile(geometryCachePathAbsolute, reloadInfo.spirV.geometry.value());
                addGLSLIncludesFileIndicesToSet(shaderPaths.geometry.value(), &shaderSrcInfo.includeFileIndices);
            }
            //tessellation control
            if (hasTessellationControlShader) {
                const fs::path tessellationControlCachePathAbsolute = m_filePathsAbsolute[shaderSrcInfo.loadInfo.tessellationControl.value().spirvCacheFileIndex];
                writeBinaryFile(tessellationControlCachePathAbsolute, reloadInfo.spirV.tessellationControl.value());
                addGLSLIncludesFileIndicesToSet(shaderPaths.tessellationControl.value(), &shaderSrcInfo.includeFileIndices);
            }
            //tessellation control
            if (hasTessellationEvauluationShader) {
                const fs::path tessellationEvaluationCachePathAbsolute = m_filePathsAbsolute[shaderSrcInfo.loadInfo.tessellationEvaluation.value().spirvCacheFileIndex];
                writeBinaryFile(tessellationEvaluationCachePathAbsolute, reloadInfo.spirV.tessellationEvaluation.value());
                addGLSLIncludesFileIndicesToSet(shaderPaths.tessellationEvaluation.value(), &shaderSrcInfo.includeFileIndices);
            }
			else {
				//update latest cache update point
				//even if load/compilation is not succesfull we don't want to keep failing every frame before problem is solved
				m_fileLastChanges[shaderSrcInfo.loadInfo.vertex.spirvCacheFileIndex] = fs::_File_time_clock::now();
				m_fileLastChanges[shaderSrcInfo.loadInfo.fragment.spirvCacheFileIndex] = fs::_File_time_clock::now();
				if (hasGeometryShader) {
					m_fileLastChanges[shaderSrcInfo.loadInfo.geometry.value().spirvCacheFileIndex] = fs::_File_time_clock::now();
				}
				if (hasTessellationControlShader) {
					m_fileLastChanges[shaderSrcInfo.loadInfo.tessellationControl.value().spirvCacheFileIndex] = fs::_File_time_clock::now();
				}
				if (hasTessellationEvauluationShader) {
					m_fileLastChanges[shaderSrcInfo.loadInfo.tessellationEvaluation.value().spirvCacheFileIndex] = fs::_File_time_clock::now();
				}
			}
        }
    }
	m_outOfDateGraphicIndices.clear();
	m_outOfDateListsMutex.unlock();
    return reloadList;
}

void ShaderFileManager::updateFileLastChangeTimes() {
	assert(m_filePathsAbsolute.size() == m_fileLastChanges.size());
	for (size_t i = 0; i < m_filePathsAbsolute.size(); i++) {
		const fs::path filePath = m_filePathsAbsolute[i];
		fs::file_time_type lastChange;
		if (checkLastChangeTime(filePath, &lastChange)) {
			//if compilation of spirv cache files fails the last change time is still updated to avoid failing with same problem every frame
			//use max to avoid overwriting this date
			m_fileLastChanges[i] = std::max(m_fileLastChanges[i], lastChange);
		}
	}
}

size_t ShaderFileManager::addFilePath(const fs::path& filePathAbsolute) {
    if (m_pathToFileIndex.find(filePathAbsolute.string()) == m_pathToFileIndex.end()) {
        const size_t fileIndex = m_filePathsAbsolute.size();
        m_pathToFileIndex[filePathAbsolute.string()] = fileIndex;
        m_filePathsAbsolute.push_back(filePathAbsolute);

        fs::file_time_type lastChangeTime;
        const bool readSuccess = checkLastChangeTime(filePathAbsolute, &lastChangeTime);
        if (!readSuccess) {
            lastChangeTime = fs::file_time_type::time_point();
        }
        m_fileLastChanges.push_back(lastChangeTime);
        return fileIndex;
    }
    else {
        return m_pathToFileIndex[filePathAbsolute.string()];
    }
}

bool ShaderFileManager::isComputeShaderCacheOutOfDate(const ComputeShaderSourceInfo& shaderSrcInfo) const{
    //search for latest change of source files
    const size_t shaderFileIndex = shaderSrcInfo.loadInfo.shaderFileIndex;
    fs::file_time_type latestSrcChange = m_fileLastChanges[shaderFileIndex];
    for (const size_t includeFileIndex : shaderSrcInfo.includeFileIndices) {
        latestSrcChange = std::max(latestSrcChange, m_fileLastChanges[includeFileIndex]);
    }
    //check for latest change of spirv cache file
    const size_t spirvCacheFileIndex = shaderSrcInfo.loadInfo.spirvCacheFileIndex;
    const fs::file_time_type latestSpirvCacheChange = m_fileLastChanges[spirvCacheFileIndex];

    return latestSpirvCacheChange < latestSrcChange;
}

bool ShaderFileManager::areGraphicShadersCachesOutOfDate(const GraphicShaderSourceInfo& shaderSrcInfo) const{
    //helper function
    //look up src and cache last file changes from load info using latestFileChanges and max with existing values
    const auto latestSrcAndCacheFileChanges = [](const ShaderLoadInfo& loadInfo, fs::file_time_type* outLatestSrcChangeTime,
        fs::file_time_type* outLatestCacheChangeTime, const std::vector<fs::file_time_type>& latestFileChanges) {
        //src file
        const size_t srcFileIndex = loadInfo.shaderFileIndex;
        *outLatestSrcChangeTime = std::max(*outLatestSrcChangeTime, latestFileChanges[srcFileIndex]);
        //cache file
        size_t cacheFileIndex = loadInfo.spirvCacheFileIndex;
        *outLatestCacheChangeTime = std::max(*outLatestCacheChangeTime, latestFileChanges[cacheFileIndex]);
    };

    //search for latest change of source files and spirvCache
    fs::file_time_type latestSrcChange = fs::file_time_type::min();
    fs::file_time_type latestSpirvCacheChange = fs::file_time_type::min();
    //vertex
    latestSrcAndCacheFileChanges(shaderSrcInfo.loadInfo.vertex,
        &latestSrcChange, &latestSpirvCacheChange, m_fileLastChanges);
    //fragment
    latestSrcAndCacheFileChanges(shaderSrcInfo.loadInfo.fragment,
        &latestSrcChange, &latestSpirvCacheChange, m_fileLastChanges);
    //geometry
    if (shaderSrcInfo.loadInfo.geometry.has_value()) {
        latestSrcAndCacheFileChanges(shaderSrcInfo.loadInfo.geometry.value(),
            &latestSrcChange, &latestSpirvCacheChange, m_fileLastChanges);
    }
    //tesselation control
    if (shaderSrcInfo.loadInfo.tessellationControl.has_value()) {
        latestSrcAndCacheFileChanges(shaderSrcInfo.loadInfo.tessellationControl.value(),
            &latestSrcChange, &latestSpirvCacheChange, m_fileLastChanges);
    }
    //tesselation evaluation
    if (shaderSrcInfo.loadInfo.tessellationEvaluation.has_value()) {
        latestSrcAndCacheFileChanges(shaderSrcInfo.loadInfo.tessellationEvaluation.value(),
            &latestSrcChange, &latestSpirvCacheChange, m_fileLastChanges);
    }
    //include files
    for (const size_t includeFileIndex : shaderSrcInfo.includeFileIndices) {
        latestSrcChange = std::max(latestSrcChange, m_fileLastChanges[includeFileIndex]);
    }

    return latestSpirvCacheChange < latestSrcChange;
}

ShaderLoadInfo ShaderFileManager::loadInfoFromShaderDescription(const ShaderDescription& shaderDesc) {
    ShaderLoadInfo loadInfo;
    const fs::path pathAbsolute = relativeShaderPathToAbsolute(shaderDesc.srcPathRelative);
    //shader file index
    {
        const size_t fileIndex = addFilePath(pathAbsolute);
        loadInfo.shaderFileIndex = fileIndex;
    }
    //cache file index
    {
        const fs::path cachePathAbsolute = shaderCachePathFromRelative(shaderDesc.srcPathRelative);
        const size_t cacheIndex = addFilePath(cachePathAbsolute);
        loadInfo.spirvCacheFileIndex = cacheIndex;
    }
    return loadInfo;
}

void ShaderFileManager::addGLSLIncludesFileIndicesToSet(const fs::path& shaderPathAbsolute, std::unordered_set<size_t>* outIndexSet){
    assert(outIndexSet != nullptr);
    //TODO: reuse loaded glsl code
    std::vector<char> glsl;
    if (!loadTextFile(shaderPathAbsolute, &glsl)) {
        std::cout << "Failed to load shader GLSL, skipping includes: " << shaderPathAbsolute << "\n";
        //glsl will stay empty, so include files just be empty
    }
    const std::vector<fs::path> includeFiles = parseIncludePathsFromGLSL(glsl);
    for (const auto& includePath : includeFiles) {
        const size_t cacheIndex = addFilePath(includePath);
        outIndexSet->insert(cacheIndex);
    }
}