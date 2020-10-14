#pragma once
#include "pch.h"
#include "Common/MeshData.h"

//filename is relative to resource directory
void saveBinaryMeshData(const std::filesystem::path& filename, const std::vector<MeshBinary>& meshes);
//filename is relative to resource directory
bool loadBinaryMeshData(const std::filesystem::path& filename, std::vector<MeshBinary>* outMeshes);