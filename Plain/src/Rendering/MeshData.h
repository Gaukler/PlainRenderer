#pragma once
#include "pch.h"
#include "Rendering/ResourceDescriptions.h"
#include <glm/common.hpp>

struct TexturePaths {
    std::filesystem::path albedoTexturePath;
    std::filesystem::path normalTexturePath;
    std::filesystem::path specularTexturePath;
};

struct MeshData {
    std::vector<uint32_t>  indices;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;
    std::vector<glm::vec3> bitangents;
    std::vector<glm::vec2> uvs;

    TexturePaths material;
};