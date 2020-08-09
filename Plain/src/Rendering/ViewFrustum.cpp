#include "pch.h"
#include "ViewFrustum.h"

/*
=========
computeViewFrustum
=========
*/
ViewFrustum computeViewFrustum(const Camera& camera) {
    //reference: http://www.lighthouse3d.com/tutorials/view-frustum-culling/view-frustums-shape/
    //reference: http://www.lighthouse3d.com/tutorials/view-frustum-culling/geometric-approach-extracting-the-planes/
    const CameraExtrinsic& extrinsic = camera.extrinsic;
    const CameraIntrinsic& intrinsic = camera.intrinsic;

    //looking towards negative axis
    const glm::vec3 nearPlaneCenter = extrinsic.position - extrinsic.forward * intrinsic.near;
    const glm::vec3 farPlaneCenter = extrinsic.position - extrinsic.forward * intrinsic.far;

    const float tanFoV = glm::tan(glm::radians(camera.intrinsic.fov) * 0.5f);
    const float heightNear = tanFoV * intrinsic.near;
    const float heightFar = tanFoV * intrinsic.far;

    const float widthNear = heightNear * intrinsic.aspectRatio;
    const float widthFar = heightFar * intrinsic.aspectRatio;

    const glm::vec3& up = extrinsic.up;
    const glm::vec3& right = extrinsic.right;
    ViewFrustum frustum;
    frustum.f_u_r = farPlaneCenter + up * heightFar + right * widthFar;
    frustum.f_u_l = farPlaneCenter + up * heightFar - right * widthFar;
    frustum.f_l_r = farPlaneCenter - up * heightFar + right * widthFar;
    frustum.f_l_l = farPlaneCenter - up * heightFar - right * widthFar;

    frustum.n_u_r = nearPlaneCenter + up * heightNear + right * widthNear;
    frustum.n_u_l = nearPlaneCenter + up * heightNear - right * widthNear;
    frustum.n_l_r = nearPlaneCenter - up * heightNear + right * widthNear;
    frustum.n_l_l = nearPlaneCenter - up * heightNear - right * widthNear;

    return frustum;
}

std::vector<glm::vec3> frustumToLineStrip(const ViewFrustum& frustum) {
    std::vector<glm::vec3> vertices(verticesInViewFrustumLineStrip);
    //make sure that there are no diagonals -> only change plane or horizontal or vertical
    //near plane
    vertices[0] = glm::vec3(frustum.n_u_r);
    vertices[1] = glm::vec3(frustum.n_l_r);
    vertices[2] = glm::vec3(frustum.n_l_l);
    vertices[3] = glm::vec3(frustum.n_u_l);
    vertices[4] = glm::vec3(frustum.n_u_r);

    //far plane
    vertices[5] = glm::vec3(frustum.f_u_r);
    vertices[6] = glm::vec3(frustum.f_l_r);
    vertices[7] = glm::vec3(frustum.f_l_l);
    vertices[8] = glm::vec3(frustum.f_u_l);
    vertices[9] = glm::vec3(frustum.f_u_r);

    //top plane
    vertices[10] = glm::vec3(frustum.f_u_l);
    vertices[11] = glm::vec3(frustum.n_u_l);
    vertices[12] = glm::vec3(frustum.n_u_r);
    vertices[13] = glm::vec3(frustum.f_u_r);
    vertices[14] = glm::vec3(frustum.f_u_l);

    //bot plane
    vertices[15] = glm::vec3(frustum.f_l_l);
    vertices[16] = glm::vec3(frustum.n_l_l);
    vertices[17] = glm::vec3(frustum.n_l_r);
    vertices[18] = glm::vec3(frustum.f_l_r);
    vertices[19] = glm::vec3(frustum.f_l_l);

    return vertices;
}