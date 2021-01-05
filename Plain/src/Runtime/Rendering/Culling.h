#pragma once
#include "pch.h"
#include "ViewFrustum.h"
#include "AABB.h"

bool isAxisAlignedBoundingBoxIntersectingViewFrustum(const ViewFrustum& frustum, const AxisAlignedBoundingBox& bb);