#pragma once
#include "pch.h"
#include "sdfUtilities.h"

AxisAlignedBoundingBox padSDFBoundingBox(const AxisAlignedBoundingBox& bb) {
    //padding avoids the sdf texture clamping resulting in hits outside or at the edge of the volume
    //additionally it ensures that the outside texels are outside the scene and dont intersect with it, causing problems with the surface
    //however excessive padding reduces the effective resolution
    glm::vec3 padding = 0.075f * (bb.max - bb.min);

    const float minPadding = 0.5f;
    padding = glm::max(padding, glm::vec3(minPadding));

    AxisAlignedBoundingBox padded;
    padded.min = bb.min - padding;
    padded.max = bb.max + padding;

    return padded;
}