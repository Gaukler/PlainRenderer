#pragma once
#include "pch.h"
#include "Camera.h"

//stores view frustum points
//naming scheme: plane(n:near, f:far)_vertical(u:upper,l: lower)_horizontal(l:left, r:right)
struct ViewFrustum {
    glm::vec3 n_u_l;
    glm::vec3 n_u_r;
    glm::vec3 n_l_l;
    glm::vec3 n_l_r;

    glm::vec3 f_u_l;
    glm::vec3 f_u_r;
    glm::vec3 f_l_l;
    glm::vec3 f_l_r;
};

ViewFrustum computeViewFrustum(const Camera& camera);

const uint32_t verticesInViewFrustumLineStrip = 20;
std::vector<glm::vec3> frustumToLineStrip(const ViewFrustum& frustum);