#pragma once
#include "pch.h"
#include "Common/MeshData.h"

std::vector<AxisAlignedBoundingBox> AABBListFromMeshes(const std::vector<MeshData>& meshes);
void computeTangentBitangent(MeshData* outMeshData);
MeshData buildIndexedData(const MeshData& rawData);
std::vector<MeshBinary> meshesToBinary(const std::vector<MeshData>& meshes, const std::vector<AxisAlignedBoundingBox>& AABBList);