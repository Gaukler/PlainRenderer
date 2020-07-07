#pragma once
#include "pch.h"
#include "Rendering/ResourceDescriptions.h"
#include <glm/common.hpp>

struct Material {
    ImageDescription diffuseTexture;
    ImageDescription normalTexture;
    ImageDescription metalicTexture;
    ImageDescription roughnessTexture;
};

struct MeshData {
    std::vector<uint32_t>  indices;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> tangents;
    std::vector<glm::vec3> bitangents;
    std::vector<glm::vec2> uvs;

    Material material;

    /*
    meshes can be created without using material data
    these must only be used with graphic passes which do not use the material system, like the skybox
    */
    bool    useMaterial = true;
};