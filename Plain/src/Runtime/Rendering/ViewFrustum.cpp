#include "pch.h"
#include "ViewFrustum.h"

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
    frustum.points.r_u_f = farPlaneCenter + up * heightFar + right * widthFar;
    frustum.points.l_u_f = farPlaneCenter + up * heightFar - right * widthFar;
    frustum.points.r_l_f = farPlaneCenter - up * heightFar + right * widthFar;
    frustum.points.l_l_f = farPlaneCenter - up * heightFar - right * widthFar;

    frustum.points.r_u_n = nearPlaneCenter + up * heightNear + right * widthNear;
    frustum.points.l_u_n = nearPlaneCenter + up * heightNear - right * widthNear;
    frustum.points.r_l_n = nearPlaneCenter - up * heightNear + right * widthNear;
    frustum.points.l_l_n = nearPlaneCenter - up * heightNear - right * widthNear;

    frustum.normals = computeViewFrustumNormals(frustum.points);
    
    return frustum;
}

ViewFrustumNormals computeViewFrustumNormals(const ViewFrustumPoints& p) {
    ViewFrustumNormals normals;
    normals.top = glm::normalize(glm::cross(p.r_u_n - p.l_u_n, p.r_u_f - p.r_u_n));
    normals.bot = glm::normalize(glm::cross(p.r_l_f - p.r_l_n, p.r_l_n - p.l_l_n));

    normals.right = glm::normalize(glm::cross(p.r_l_f - p.r_l_n, p.r_u_n - p.r_l_n));
    normals.left = glm::normalize(glm::cross(p.l_u_n - p.l_l_n, p.l_l_f - p.l_l_n));

    normals.near = glm::normalize(glm::cross(p.r_l_n - p.l_l_n, p.r_u_n - p.r_l_n));
    normals.far = glm::normalize(glm::cross(p.r_u_f - p.r_l_f, p.r_l_f - p.l_l_f));

    return normals;
}

void frustumToLineMesh(const ViewFrustum& frustum,
    std::vector<glm::vec3>* outPositions, std::vector<uint32_t>* outIndices) {

    //positions
    outPositions->resize(positionsInViewFrustumLineMesh);
    const auto& p = frustum.points;
    
    //indices corresponding to points defined to make index creation more manageable
    //corners
    (*outPositions)[0] = p.l_l_n; const uint32_t l_l_n = 0;
    (*outPositions)[1] = p.l_l_f; const uint32_t l_l_f = 1;
    (*outPositions)[2] = p.l_u_n; const uint32_t l_u_n = 2;
    (*outPositions)[3] = p.l_u_f; const uint32_t l_u_f = 3;
    (*outPositions)[4] = p.r_l_n; const uint32_t r_l_n = 4;
    (*outPositions)[5] = p.r_l_f; const uint32_t r_l_f = 5;
    (*outPositions)[6] = p.r_u_n; const uint32_t r_u_n = 6;
    (*outPositions)[7] = p.r_u_f; const uint32_t r_u_f = 7;

    //normals
    const glm::vec3 nearCenter  = (p.l_u_n + p.l_l_n + p.r_u_n + p.r_l_n) * 0.25f;
    const glm::vec3 farCenter   = (p.l_u_f + p.l_l_f + p.r_u_f + p.r_l_f) * 0.25f;
    const glm::vec3 rightCenter = (p.r_u_n + p.r_u_f + p.r_l_n + p.r_l_f) * 0.25f;
    const glm::vec3 leftCenter  = (p.l_u_n + p.l_u_f + p.l_l_n + p.l_l_f) * 0.25f;
    const glm::vec3 topCenter   = (p.l_u_n + p.r_u_f + p.r_u_n + p.l_u_f) * 0.25f;
    const glm::vec3 botCenter   = (p.l_l_n + p.r_l_f + p.r_l_n + p.l_l_f) * 0.25f;

    const auto& n = frustum.normals;
    const float normalLength = 3.f;

    (*outPositions)[8] = nearCenter;    const uint32_t nearCenterIndex = 8;
    (*outPositions)[9]  = nearCenter + n.near * normalLength;

    (*outPositions)[10] = farCenter;    const uint32_t farCenterIndex = 10;
    (*outPositions)[11] = farCenter + n.far * normalLength;

    (*outPositions)[12] = rightCenter;  const uint32_t rightCenterIndex = 12;
    (*outPositions)[13] = rightCenter + n.right * normalLength;

    (*outPositions)[14] = leftCenter;   const uint32_t leftCenterIndex = 14;
    (*outPositions)[15] = leftCenter + n.left * normalLength;

    (*outPositions)[16] = topCenter;    const uint32_t topCenterIndex = 16;
    (*outPositions)[17] = topCenter + n.top * normalLength;

    (*outPositions)[18] = botCenter;    const uint32_t botCenterIndex = 18;
    (*outPositions)[19] = botCenter + n.bot * normalLength;

    //indices
    outIndices->resize(indicesInViewFrustumLineMesh);

    //connect center with corners to make normal directions more readable
    //back quad
    (*outIndices)[0] = l_l_n;
    (*outIndices)[1] = l_u_n;
    (*outIndices)[2] = l_l_n;
    (*outIndices)[3] = r_l_n;
    (*outIndices)[4] = r_u_n;
    (*outIndices)[5] = r_l_n;
    (*outIndices)[6] = l_u_n;
    (*outIndices)[7] = r_u_n;

    //connection to center
    (*outIndices)[8] = nearCenterIndex;
    (*outIndices)[9] = r_u_n;
    (*outIndices)[10] = nearCenterIndex;
    (*outIndices)[11] = l_l_n;
    (*outIndices)[12] = nearCenterIndex;
    (*outIndices)[13] = l_u_n;
    (*outIndices)[14] = nearCenterIndex;
    (*outIndices)[15] = r_l_n;

    //front quad
    (*outIndices)[16] = l_l_f;
    (*outIndices)[17] = l_u_f;
    (*outIndices)[18] = l_l_f;
    (*outIndices)[19] = r_l_f;
    (*outIndices)[20] = r_u_f;
    (*outIndices)[21] = r_l_f;
    (*outIndices)[22] = l_u_f;
    (*outIndices)[23] = r_u_f;

    //connection to center
    (*outIndices)[24] = farCenterIndex;
    (*outIndices)[25] = l_l_f;
    (*outIndices)[26] = farCenterIndex;
    (*outIndices)[27] = r_u_f;
    (*outIndices)[28] = farCenterIndex;
    (*outIndices)[29] = r_l_f;
    (*outIndices)[30] = farCenterIndex;
    (*outIndices)[31] = l_u_f;


    //top connections
    (*outIndices)[32] = l_u_f;
    (*outIndices)[33] = l_u_n;
    (*outIndices)[34] = r_u_f;
    (*outIndices)[35] = r_u_n;

    //connection to center
    (*outIndices)[36] = topCenterIndex;
    (*outIndices)[37] = l_u_f;
    (*outIndices)[38] = topCenterIndex;
    (*outIndices)[39] = r_u_n;
    (*outIndices)[40] = topCenterIndex;
    (*outIndices)[41] = r_u_f;
    (*outIndices)[42] = topCenterIndex;
    (*outIndices)[43] = l_u_n;


    //bot connections
    (*outIndices)[44] = l_l_f;
    (*outIndices)[45] = l_l_n;
    (*outIndices)[46] = r_l_f;
    (*outIndices)[47] = r_l_n;

    //connection to center
    (*outIndices)[48] = botCenterIndex;
    (*outIndices)[49] = l_l_f;
    (*outIndices)[50] = botCenterIndex;
    (*outIndices)[51] = r_l_n;
    (*outIndices)[52] = botCenterIndex;
    (*outIndices)[53] = r_l_f;
    (*outIndices)[54] = botCenterIndex;
    (*outIndices)[55] = l_l_n;

    //right connection to center
    (*outIndices)[56] = rightCenterIndex;
    (*outIndices)[57] = r_l_f;
    (*outIndices)[58] = rightCenterIndex;
    (*outIndices)[59] = r_u_n;
    (*outIndices)[60] = rightCenterIndex;
    (*outIndices)[61] = r_u_f;
    (*outIndices)[62] = rightCenterIndex;
    (*outIndices)[63] = r_l_n;

    //left connection to center
    (*outIndices)[64] = leftCenterIndex;
    (*outIndices)[65] = l_l_f;
    (*outIndices)[66] = leftCenterIndex;
    (*outIndices)[67] = l_u_n;
    (*outIndices)[68] = leftCenterIndex;
    (*outIndices)[69] = l_u_f;
    (*outIndices)[70] = leftCenterIndex;
    (*outIndices)[71] = l_l_n;

    //normal indices
    (*outIndices)[72] = 8;
    (*outIndices)[73] = 9;

    (*outIndices)[74] = 10;
    (*outIndices)[75] = 11;

    (*outIndices)[76] = 12;
    (*outIndices)[77] = 13;

    (*outIndices)[78] = 14;
    (*outIndices)[79] = 15;

    (*outIndices)[80] = 16;
    (*outIndices)[81] = 17;

    (*outIndices)[82] = 18;
    (*outIndices)[83] = 19;
}

std::array<glm::vec3, 8> getFrustumPoints(const ViewFrustum& frustum) {
    return {
        frustum.points.l_l_f,
        frustum.points.l_l_n,
        frustum.points.r_l_f,
        frustum.points.r_l_n,
        frustum.points.l_u_f,
        frustum.points.l_u_n,
        frustum.points.r_u_f,
        frustum.points.r_u_n,
    };
}

ViewFrustum computeOrthogonalFrustumFittedToCamera(const ViewFrustum& cameraFrustum, const glm::vec3& lightDirection) {

    //reference: https://developer.download.nvidia.com/SDK/10.5/opengl/src/cascaded_shadow_maps/doc/cascaded_shadow_maps.pdf
    //reference: https://lwjglgamedev.gitbooks.io/3d-game-development-with-lwjgl/content/chapter26/chapter26.html
    glm::vec3 up = abs(lightDirection.y) < 0.999f ? glm::vec3(0.f, -1.f, 0.f) : glm::vec3(0.f, 0.f, -1.f);

    const glm::mat4 V = glm::lookAt(-lightDirection, glm::vec3(0.f), up);
    glm::vec3 maxP = glm::vec3(std::numeric_limits<float>::min());
    glm::vec3 minP = glm::vec3(std::numeric_limits<float>::max());

    for (const auto& p : getFrustumPoints(cameraFrustum)) {
        const glm::vec3 pTransformed = V * glm::vec4(p, 1.f);
        minP = glm::min(minP, pTransformed);
        maxP = glm::max(maxP, pTransformed);
    }
    
    glm::vec3 scale = 2.f / (maxP - minP);
    const glm::vec3 offset = -0.5f * (maxP + minP) * scale;

    glm::mat4 clip(1.f);
    clip[0][0] = scale.x;
    clip[1][1] = scale.y;
    clip[2][2] = scale.z;
    clip[3] = glm::vec4(offset, 1);

    glm::mat4 clipToWorld = glm::inverse(clip * V);

    ViewFrustum result;
    result.points.l_l_n = clipToWorld * glm::vec4(-1,  1, -1, 1);
    result.points.r_l_n = clipToWorld * glm::vec4( 1,  1, -1, 1);
    result.points.l_u_n = clipToWorld * glm::vec4(-1, -1, -1, 1);
    result.points.r_u_n = clipToWorld * glm::vec4( 1, -1, -1, 1);
    result.points.l_l_f = clipToWorld * glm::vec4(-1,  1,  1, 1);
    result.points.r_l_f = clipToWorld * glm::vec4( 1,  1,  1, 1);
    result.points.l_u_f = clipToWorld * glm::vec4(-1, -1,  1, 1);
    result.points.r_u_f = clipToWorld * glm::vec4( 1, -1,  1, 1);

    result.normals = computeViewFrustumNormals(result.points);

    return result;
}