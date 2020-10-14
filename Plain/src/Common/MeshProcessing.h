#pragma once
#include "pch.h"
#include "Runtime/Rendering/MeshData.h"

void computeTangentBitangent(MeshData* outMeshData);
MeshData buildIndexedData(const MeshData& rawData);
std::vector<MeshBinary> meshesToBinary(const std::vector<MeshData>& meshes);