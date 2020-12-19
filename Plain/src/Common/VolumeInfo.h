#pragma once
#include "pch.h"
#include "BoundingBox.h"

struct VolumeInfo {
    glm::vec4 extends = glm::vec4(0.f);
    glm::vec4 offset = glm::vec4(0.f);
};

VolumeInfo volumeInfoFromBoundingBox(const AxisAlignedBoundingBox& bb);