#pragma once
#include "pch.h"

//path is needed to infer shader type and include directory
bool compileGLSLToSPIRV(const std::vector<char>& code, const std::filesystem::path& absolutePath, std::vector<uint32_t>* outSpirV);