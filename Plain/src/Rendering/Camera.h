#pragma once
#include "pch.h"

struct CameraExtrinsic {
    glm::vec3 position = glm::vec3(0.f, -1.f, -5.f);
    glm::vec3 forward;
    glm::vec3 right;
    glm::vec3 up;
};

struct CameraIntrinsic {
    float fov = 35.f;
    float aspectRatio;
};


struct Camera {
    CameraExtrinsic extrinsic;
    CameraIntrinsic intrinsic;
};