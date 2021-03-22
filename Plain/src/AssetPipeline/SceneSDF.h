#pragma once
#include "pch.h"
#include "Common/MeshData.h"
#include "ImageDescription.h"

struct SceneSDFTextures {
	std::vector<ImageDescription> descriptions;
	std::vector<std::vector<uint8_t>> data;
};
SceneSDFTextures computeSceneSDFTextures(const std::vector<MeshData>& meshes, const std::vector<AxisAlignedBoundingBox>& AABBList);