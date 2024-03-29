#pragma once
#include "pch.h"
#include <glm/common.hpp>
#include "AABB.h"

struct TexturePaths {
    std::filesystem::path albedoTexturePath;
    std::filesystem::path normalTexturePath;
    std::filesystem::path specularTexturePath;
    std::filesystem::path sdfTexturePath;
};

struct MeshData {
    std::vector<uint32_t>  indices;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;
    std::vector<glm::vec3> bitangents;
    std::vector<glm::vec2> uvs;

    TexturePaths texturePaths;
    glm::vec3 meanAlbedo = glm::vec3(0.5f);
};

//formated to be consumed directly by render backend
struct MeshBinary {
    uint32_t                indexCount = 0;
    uint32_t                vertexCount = 0;
    AxisAlignedBoundingBox  boundingBox;
    TexturePaths            texturePaths;
    glm::vec3               meanAlbedo = glm::vec3(0.5f);
    std::vector<uint16_t>   indexBuffer;    //stored as 16 or 32 bit unsigned int
    std::vector<uint8_t>    vertexBuffer;
};