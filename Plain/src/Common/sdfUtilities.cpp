#pragma once
#include "pch.h"
#include "sdfUtilities.h"

std::filesystem::path binaryToSDFPath(const std::filesystem::path binaryPathRelative) {
    std::filesystem::path sdfPathRelative = binaryPathRelative;
    sdfPathRelative.replace_extension(); //remove extension
    sdfPathRelative += "_sdf";
    sdfPathRelative.replace_extension("dds");
    return sdfPathRelative;
}

AxisAlignedBoundingBox padSDFBoundingBox(const AxisAlignedBoundingBox& bb) {
	//padding avoids the sdf texture clamping resulting in hits outside or at the edge of the volume
	//additionally it ensures that the outside texels are outside the scene and dont intersect with it, causing problems with the surface
	//however excessive padding reduces the effective resolution
	const float padding = 0.75f;

	AxisAlignedBoundingBox padded;
	padded.min = bb.min - padding;
	padded.max = bb.max + padding;

	return padded;
}