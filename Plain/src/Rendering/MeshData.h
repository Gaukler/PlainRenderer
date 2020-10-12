#pragma once
#include "pch.h"
#include <glm/common.hpp>
#include "Rendering/ResourceDescriptions.h"
#include "BoundingBox.h"

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

    TexturePaths texturePaths;
};

//formated to be consumed directly by render backend
struct MeshBinary {
    uint32_t                indexCount;
    uint32_t                vertexCount;
    AxisAlignedBoundingBox  boundingBox;
    TexturePaths            texturePaths;
    std::vector<uint16_t>   indexBuffer;    //stored as 16 or 32 bit unsigned int
    std::vector<uint8_t>    vertexBuffer;
};

struct Material {
    ImageHandle diffuseTexture;
    ImageHandle normalTexture;
    ImageHandle specularTexture;
};