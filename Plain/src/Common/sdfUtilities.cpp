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

AxisAlignedBoundingBox padSDFBoundingBox(const AxisAlignedBoundingBox& bb, const glm::ivec3 resolution) {
	//pad min and max by cell size
	const glm::vec3 extends = bb.max - bb.min;
	const glm::vec3 cellSize = extends / glm::vec3(resolution);

	AxisAlignedBoundingBox padded;
	padded.min = bb.min - 2.f * cellSize;
	padded.max = bb.max + 2.f * cellSize;

	return padded;
}