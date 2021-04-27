#pragma once
#include "pch.h"
#include "RenderHandles.h"

// texture indices for direct use in shader, index into global texture array
struct Material {
    uint32_t albedoTextureIndex = 0;
    uint32_t normalTextureIndex = 0;
    uint32_t specularTextureIndex = 0;
};

struct MeshFrontend {
    MeshHandle              backendHandle;
    int                     sdfTextureIndex = 0;
    glm::vec3               meanAlbedo = glm::vec3(0.5f);
    Material                material;
    AxisAlignedBoundingBox  localBB;
};