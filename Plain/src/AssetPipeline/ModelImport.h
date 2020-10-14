#pragma once
#include "pch.h"
#include "Runtime/Rendering/MeshData.h"

//filename is relative to resource directory
bool loadModelOBJ(const std::filesystem::path& filename, std::vector<MeshData>* outData);