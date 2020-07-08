#pragma once
#include "pch.h"

//path is needed to infer shader type and include directory
std::vector<uint32_t> compileGLSLToSPIRV(const std::vector<char>& code, const std::filesystem::path& absolutePath);