#pragma once
#include "pch.h"

struct AxisAlignedBoundingBox {
    glm::vec3 min = glm::vec3(0.f);
    glm::vec3 max = glm::vec3(0.f);
};

AxisAlignedBoundingBox axisAlignedBoundingBoxFromPositions(const std::vector<glm::vec3>& positions);
AxisAlignedBoundingBox axisAlignedBoundingBoxTransformed(const AxisAlignedBoundingBox& bb, const glm::mat4& m);

std::array<glm::vec3, 8> getAxisAlignedBoundingBoxPoints(const AxisAlignedBoundingBox& bb);

AxisAlignedBoundingBox combineAxisAlignedBoundingBoxes(const std::vector<AxisAlignedBoundingBox>& bbs);

//vertex count that axisAlignedBoundingBoxToLineStrip produces
const uint32_t axisAlignedBoundingBoxPositionsPerMesh = 8;
const uint32_t axisAlignedBoundingBoxIndicesPerMesh = 24;

//useful for debug rendering, indices are line list
void axisAlignedBoundingBoxToLineMesh(const AxisAlignedBoundingBox& bb, 
    std::vector<glm::vec3>* outPositions, std::vector<uint32_t>* outIndices);

//used for shadow rendering around area
//for fitting to camera frustum the exact frustum should be used, not the bounding box
glm::mat4 viewProjectionMatrixAroundBB(const AxisAlignedBoundingBox& bb, const glm::vec3& viewDirection);

bool isPointInAABB(const glm::vec3 p, const glm::vec3 min, const glm::vec3 max);