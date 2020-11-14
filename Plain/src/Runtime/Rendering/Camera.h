#pragma once
#include "pch.h"

struct CameraExtrinsic {
    glm::vec3 position = glm::vec3(0.f, -1.f, -5.f);
    glm::vec3 forward = glm::vec3(0.f, 0.f, -1.f);
    glm::vec3 right = glm::vec3(1.f, 0.f, 0.f);
    glm::vec3 up = glm::vec3(0.f, -1.f, 0.f);
};

struct CameraIntrinsic {
    float fov = 35.f;
    float aspectRatio = 1.f;
    float near = 0.1f;
    float far = 300.f;
};


struct Camera {
    CameraExtrinsic extrinsic;
    CameraIntrinsic intrinsic;
};

glm::mat4 viewMatrixFromCameraExtrinsic(const CameraExtrinsic& extrinsic);
glm::mat4 projectionMatrixFromCameraIntrinsic(const CameraIntrinsic& intrinsic);