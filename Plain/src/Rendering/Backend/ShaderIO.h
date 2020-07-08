#pragma once
#include "pch.h"

/*
entire shader loading process, returns spirV code
checks for a cached up to date spirV version
only recompiles if none is found
recompiled shaders are written into the shader cache directory for next time
filename must be relative to resources\\shaders\\
*/
std::vector<uint32_t> loadShader(const std::filesystem::path& relativePath);

/*
file loader functions
*/

//loads text NOT spirV which would be binary data
std::vector<char> loadShaderTextFile(const std::filesystem::path& absolutePath);

//loads spirV binary file
std::vector<uint32_t> loadShaderSpirVFile(const std::filesystem::path& absolutePath);

//path is for final absolute path and should end with ".spv"
void writeSpirVToFile(const std::vector<uint32_t>& spirV, const std::filesystem::path absolutePath);

/*
directory utilites
*/
std::filesystem::path getShaderDirectory();
std::filesystem::path getShaderCacheDirectory();