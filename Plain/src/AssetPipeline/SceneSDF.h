#pragma once
#include "pch.h"
#include "Common/MeshData.h"
#include "ImageDescription.h"

std::vector<ImageDescription> computeSceneSDFTextures(const std::vector<MeshData>& meshes, const std::vector<AxisAlignedBoundingBox>& AABBList);