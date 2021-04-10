#pragma once
#include "pch.h"
#include "Common/MeshData.h"
#include "AABB.h"

struct ObjectBinary {
    glm::mat4 modelMatrix;
    size_t meshIndex;
};

struct Scene {
    std::vector<ObjectBinary> objects;
    std::vector<MeshData> meshes;
};

struct SceneBinary {
    std::vector<ObjectBinary> objects;
    std::vector<MeshBinary> meshes;
};