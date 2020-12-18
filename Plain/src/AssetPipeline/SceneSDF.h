#pragma once
#include "pch.h"
#include "Common/MeshData.h"
#include "ImageDescription.h"

ImageDescription ComputeSceneSDFTexture(const std::vector<MeshData>& meshes, const std::vector<AxisAlignedBoundingBox>& AABBList);