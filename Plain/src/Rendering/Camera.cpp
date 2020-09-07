#include "pch.h"
#include "Camera.h"
#include "Utilities/MathUtils.h"

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
    glm::mat4 projectionMatrix = 
        glm::perspective(glm::radians(intrinsic.fov), intrinsic.aspectRatio, intrinsic.near, intrinsic.far);

    return getOpenGLToVulkanCorrectionMatrix() * projectionMatrix;
}