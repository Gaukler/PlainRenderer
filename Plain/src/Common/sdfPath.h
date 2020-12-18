#pragma once
#include "pch.h"

//transforms relative binary scene path to corresponding relative sdf texture path
//adds "_sdf" suffix and changes extensions to ".dds"
std::filesystem::path binaryToSDFPath(const std::filesystem::path binaryPathRelative);