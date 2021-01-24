#pragma once
#include "pch.h"
#include "Common/MeshData.h"

struct Object {
	glm::mat4 modelMatrix;
	size_t meshIndex;
};

struct Scene {
	std::vector<Object> objects;
	std::vector<MeshData> meshes;
};

struct SceneBinary {
	std::vector<Object> objects;
	std::vector<MeshBinary> meshes;
};