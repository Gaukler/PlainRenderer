#pragma once
#include "pch.h"
#include "Common/MeshData.h"

//filename is relative to resource directory
bool loadModelOBJ(const std::filesystem::path& filename, std::vector<MeshData>* outData);

bool loadModelGLTF(const std::filesystem::path& filename, std::vector<MeshData>* outData);