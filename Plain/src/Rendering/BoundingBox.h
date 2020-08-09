#pragma once
#include "pch.h"

struct AxisAlignedBoundingBox {
    glm::vec3 min;
    glm::vec3 max;
};

AxisAlignedBoundingBox axisAlignedBoundingBoxFromPositions(const std::vector<glm::vec3>& positions);
AxisAlignedBoundingBox axisAlignedBoundingBoxTransformed(const AxisAlignedBoundingBox& bb, const glm::mat4& m);

//vertex count that axisAlignedBoundingBoxToLineStrip produces
const uint32_t axisAlignedBoundingBoxVerticesPerMesh = 20;

//useful for debug rendering
std::vector<glm::vec3> axisAlignedBoundingBoxToLineStrip(const AxisAlignedBoundingBox& bb);