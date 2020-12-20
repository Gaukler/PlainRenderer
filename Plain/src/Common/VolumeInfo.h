#pragma once
#include "pch.h"
#include "BoundingBox.h"

//using vec4s because of gpu buffer padding
struct VolumeInfo {
    glm::vec4 extends = glm::vec4(0.f);
    glm::vec4 offset = glm::vec4(0.f);
};

VolumeInfo volumeInfoFromBoundingBox(const AxisAlignedBoundingBox& bb);