#pragma once
#include "pch.h"
#include "vulkan/vulkan.h"

#include "Rendering/MeshData.h"

std::vector<MeshData> loadModel(const std::filesystem::path& filename);
void        computeTangentBitangent(MeshData* outMeshData);
MeshData buildIndexedData(const MeshData& rawData);