#pragma once
#include "pch.h"
#include "Resources.h"
#include "Rendering/ResourceDescriptions.h"

/*
entire shader loading process, returns spirV code
checks for a cached up to date spirV version
only recompiles if none is found
recompiled shaders are written into the shader cache directory for next time
filename must be relative to resources\\shaders\\
*/
bool loadShader(const std::filesystem::path& relativePath, std::vector<uint32_t>* outSpirV);

//helper that loads shaders from all supplied paths into the corresponding out struct
bool loadGraphicPassShaders(const GraphicPassShaderPaths& paths, GraphicPassShaderSpirV* outSpirV);

/*
file loader functions
*/

//loads text NOT spirV which would be binary data
bool loadShaderTextFile(const std::filesystem::path& absolutePath, std::vector<char>* outShaderCode);

//loads spirV binary file
bool loadShaderSpirVFile(const std::filesystem::path& absolutePath, std::vector<uint32_t>* outSpirV);

//path is for final absolute path and should end with ".spv"
void writeSpirVToFile(const std::vector<uint32_t>& spirV, const std::filesystem::path absolutePath);

/*
directory utilites
*/
std::filesystem::path absoluteShaderPathFromRelative(std::filesystem::path relativePath);
std::filesystem::path shaderCachePathFromRelative(std::filesystem::path relativePath);
std::filesystem::path getShaderDirectory();
std::filesystem::path getShaderCacheDirectory();