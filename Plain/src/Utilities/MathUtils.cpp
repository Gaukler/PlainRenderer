#include "pch.h"
#include "MathUtils.h"

/*
=========
directionToVector
=========
*/
glm::vec3 directionToVector(const glm::vec2 direction) {

    const float theta   = direction.y / 180.f * 3.1415f;
    const float phi     = direction.x / 180.f * 3.1415f;

    glm::vec3 vec;
    vec.x = sin(theta) * cos(phi);
    vec.y = sin(theta) * sin(phi);
    vec.z = cos(theta);
    return vec;
}

/*
=========
viewMatrixFromCameraExtrinsic
=========
*/
glm::mat4 viewMatrixFromCameraExtrinsic(const CameraExtrinsic& extrinsic) {
    glm::mat4 viewMatrix = glm::mat4(1.f);
    viewMatrix[0] = glm::vec4(extrinsic.right, 0.f);
    viewMatrix[1] = glm::vec4(extrinsic.up, 0.f);
    viewMatrix[2] = glm::vec4(extrinsic.forward, 0.f);
    viewMatrix = glm::transpose(viewMatrix);
    viewMatrix = viewMatrix * glm::translate(glm::mat4(1.f), -extrinsic.position);
    return viewMatrix;
}

/*
=========
projectionMatrixFromCameraIntrinsic
=========
*/
glm::mat4 projectionMatrixFromCameraIntrinsic(const CameraIntrinsic& intrinsic) {
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(intrinsic.fov), intrinsic.aspectRatio, 0.1f, 200.f);

    const glm::mat4 coordinateSystemCorrection = glm::mat4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, -1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 0.5f, 1.0f);

    return coordinateSystemCorrection * projectionMatrix;
}

/*
=========
mipCountFromResolution
=========
*/
uint32_t mipCountFromResolution(const uint32_t width, const uint32_t height, const uint32_t depth) {
    return 1 + std::floor(std::log2(std::max(std::max(width, height), depth)));
}