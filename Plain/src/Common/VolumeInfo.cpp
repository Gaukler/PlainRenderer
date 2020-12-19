#include "pch.h"
#include "VolumeInfo.h"

VolumeInfo volumeInfoFromBoundingBox(const AxisAlignedBoundingBox& bb) {
    VolumeInfo volumeInfo;
    volumeInfo.offset = glm::vec4((bb.max + bb.min) * 0.5f, 0.f);
    volumeInfo.extends = glm::vec4((bb.max - bb.min), 0.f);
    return volumeInfo;
}