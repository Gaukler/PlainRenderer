#pragma once
#include "pch.h"
#include "AABB.h"
#include "Rendering/RenderHandles.h"

struct SceneObject {
	MeshHandle mesh;
	size_t bbIndex;
	glm::mat4 modelMatrix;
};

struct RenderObject {
	MeshHandle mesh;
	AxisAlignedBoundingBox bbWorld;
	glm::mat4 modelMatrix;
	glm::mat4 previousModelMatrix;
};