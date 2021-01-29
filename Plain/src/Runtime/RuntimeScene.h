#pragma once
#include "pch.h"
#include "AABB.h"
#include "Rendering/RenderHandles.h"

//frontend stores backend handles with material indices
struct MeshHandleFrontend {
	uint32_t index;
};

struct SceneObject {
	MeshHandleFrontend mesh;
	size_t bbIndex;
	glm::mat4 modelMatrix;
};

struct RenderObject {
	MeshHandleFrontend mesh;
	AxisAlignedBoundingBox bbWorld;
	glm::mat4 modelMatrix;
	glm::mat4 previousModelMatrix;
};