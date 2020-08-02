#pragma once
#include "pch.h"

#include "RenderHandles.h"

/*
intermediate data format to be consumed by the render backend directly
the backend uses image handles for textures so the front end can manage duplicates
*/
struct MeshDataInternal {
    std::vector<uint32_t>  indices;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;
    std::vector<glm::vec3> bitangents;
    std::vector<glm::vec2> uvs;

    std::optional<ImageHandle> diffuseTexture;
    std::optional<ImageHandle> normalTexture;
    std::optional<ImageHandle> specularTexture;
};