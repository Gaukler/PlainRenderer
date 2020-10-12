#pragma once
#include "pch.h"
#include "vulkan/vulkan.h"

#include "Rendering/MeshData.h"

bool loadModelOBJ(const std::filesystem::path& filename, std::vector<MeshData>* outData);
void saveBinaryMeshData(const std::filesystem::path& filename, const std::vector<MeshBinary>& meshes);
bool loadBinaryMeshData(const std::filesystem::path& filename, std::vector<MeshBinary>* outMeshes);