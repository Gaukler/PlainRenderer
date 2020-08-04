#pragma once
#include "pch.h"
#include "vulkan/vulkan.h"

#include "Rendering/MeshData.h"

bool loadModel(const std::filesystem::path& filename, std::vector<MeshData>* outData);
void        computeTangentBitangent(MeshData* outMeshData);
MeshData buildIndexedData(const MeshData& rawData);