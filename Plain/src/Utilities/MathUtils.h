#pragma once
#include "pch.h"
#include <glm/common.hpp>
#include "Rendering/Camera.h"

glm::vec3 directionToVector(const glm::vec2 direction);
glm::mat4 viewMatrixFromCameraExtrinsic(const CameraExtrinsic& extrinsic);
glm::mat4 projectionMatrixFromCameraIntrinsic(const CameraIntrinsic& intrinsic);