#pragma once
#include "pch.h"
#include "ViewFrustum.h"
#include "BoundingBox.h"

bool isAxisAlignedBoundingBoxIntersectingViewFrustum(const ViewFrustum& frustum, const AxisAlignedBoundingBox& bb);