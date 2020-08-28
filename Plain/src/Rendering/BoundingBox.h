#pragma once
#include "pch.h"

struct AxisAlignedBoundingBox {
    glm::vec3 min = glm::vec3(0.f);
    glm::vec3 max = glm::vec3(0.f);
};

AxisAlignedBoundingBox axisAlignedBoundingBoxFromPositions(const std::vector<glm::vec3>& positions);
AxisAlignedBoundingBox axisAlignedBoundingBoxTransformed(const AxisAlignedBoundingBox& bb, const glm::mat4& m);

std::array<glm::vec3, 8> getAxisAlignedBoundingBoxPoints(const AxisAlignedBoundingBox& bb);

//vertex count that axisAlignedBoundingBoxToLineStrip produces
const uint32_t axisAlignedBoundingBoxPositionsPerMesh = 8;
const uint32_t axisAlignedBoundingBoxIndicesPerMesh = 24;

//useful for debug rendering, indices are line list
void axisAlignedBoundingBoxToLineMesh(const AxisAlignedBoundingBox& bb, 
    std::vector<glm::vec3>* outPositions, std::vector<uint32_t>* outIndices);