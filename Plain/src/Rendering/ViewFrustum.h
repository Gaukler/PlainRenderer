#pragma once
#include "pch.h"
#include "Camera.h"

//naming scheme: l/r(left/right)_u/l(upper/lower)_n/f(_near/far)
struct ViewFrustumPoints {
    glm::vec3 l_l_n = glm::vec3(0.f);
    glm::vec3 l_l_f = glm::vec3(0.f);
    glm::vec3 l_u_n = glm::vec3(0.f);
    glm::vec3 l_u_f = glm::vec3(0.f);
    glm::vec3 r_l_n = glm::vec3(0.f);
    glm::vec3 r_l_f = glm::vec3(0.f);
    glm::vec3 r_u_n = glm::vec3(0.f);
    glm::vec3 r_u_f = glm::vec3(0.f);
};

struct ViewFrustumNormals {
    glm::vec3 top   = glm::vec3(0.f);
    glm::vec3 bot   = glm::vec3(0.f);
    glm::vec3 right = glm::vec3(0.f);
    glm::vec3 left  = glm::vec3(0.f);
    glm::vec3 near  = glm::vec3(0.f);
    glm::vec3 far   = glm::vec3(0.f);
};

struct ViewFrustum {
    ViewFrustumPoints points;
    ViewFrustumNormals normals;
};

ViewFrustum computeViewFrustum(const Camera& camera);
ViewFrustumNormals computeViewFrustumNormals(const ViewFrustumPoints& p);

const uint32_t positionsInViewFrustumLineMesh = 20;
const uint32_t indicesInViewFrustumLineMesh = 84;

void frustumToLineMesh(const ViewFrustum& frustum, 
    std::vector<glm::vec3>* outPositions, std::vector<uint32_t>* outIndices);

//convencie function that returns all frustum points in an array for easy iterating
std::array<glm::vec3, 8> getFrustumPoints(const ViewFrustum& frustum);

ViewFrustum computeOrthogonalFrustumFittedToCamera(const ViewFrustum& cameraFrustum, const glm::vec3& lightDirection);