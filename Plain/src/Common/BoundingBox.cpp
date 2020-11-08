#include "pch.h"
#include "BoundingBox.h"

AxisAlignedBoundingBox axisAlignedBoundingBoxFromPositions(const std::vector<glm::vec3>& positions) {
    AxisAlignedBoundingBox bb;
    bb.min = glm::vec3(std::numeric_limits<float>::max());
    bb.max = glm::vec3(std::numeric_limits<float>::min());

    for (const auto& pos : positions) {
        bb.min = glm::min(bb.min, pos);
        bb.max = glm::max(bb.max, pos);
    }
    return bb;
}

AxisAlignedBoundingBox axisAlignedBoundingBoxTransformed(const AxisAlignedBoundingBox& bb, const glm::mat4& m) {
    //get object space points
    const auto bbPoints = getAxisAlignedBoundingBoxPoints(bb);

    //transform
    std::vector<glm::vec3> bbPointsTransformed(8);
    for (uint32_t i = 0; i < 8; i++) {
        const auto& p = bbPoints[i];
        bbPointsTransformed[i] = glm::vec3(m * glm::vec4(p, 1.f));
    }

    //compute new bb
    return axisAlignedBoundingBoxFromPositions(bbPointsTransformed);
}

AxisAlignedBoundingBox combineAxisAlignedBoundingBoxes(const std::vector<AxisAlignedBoundingBox>& bbs) {

    if (bbs.size() == 0) {
        return AxisAlignedBoundingBox{ glm::vec3(0), glm::vec3(0) };
    }

    AxisAlignedBoundingBox result;
    result.min = bbs[0].min;
    result.max = bbs[0].max;

    for (const auto& bb : bbs) {
        result.min = glm::min(result.min, bb.min);
        result.max = glm::max(result.max, bb.max);
    }

    return result;
}

void axisAlignedBoundingBoxToLineMesh(const AxisAlignedBoundingBox& bb, 
    std::vector<glm::vec3>* outPositions, std::vector<uint32_t>* outIndices) {

    outPositions->resize(axisAlignedBoundingBoxPositionsPerMesh);

    //indices corresponding to points defined to make index creation more manageable
    //naming scheme: l/r(left/right)_u/l(upper/lower)_f/b(front/back)
    (*outPositions)[0] = glm::vec3(bb.min.x, bb.min.y, bb.min.z); const uint32_t l_l_b = 0;
    (*outPositions)[1] = glm::vec3(bb.min.x, bb.min.y, bb.max.z); const uint32_t l_l_f = 1;
    (*outPositions)[2] = glm::vec3(bb.min.x, bb.max.y, bb.min.z); const uint32_t l_u_b = 2;
    (*outPositions)[3] = glm::vec3(bb.min.x, bb.max.y, bb.max.z); const uint32_t l_u_f = 3;
    (*outPositions)[4] = glm::vec3(bb.max.x, bb.min.y, bb.min.z); const uint32_t r_l_b = 4;
    (*outPositions)[5] = glm::vec3(bb.max.x, bb.min.y, bb.max.z); const uint32_t r_l_f = 5;
    (*outPositions)[6] = glm::vec3(bb.max.x, bb.max.y, bb.min.z); const uint32_t r_u_b = 6;
    (*outPositions)[7] = glm::vec3(bb.max.x, bb.max.y, bb.max.z); const uint32_t r_u_f = 7;

    //indices
    outIndices->resize(axisAlignedBoundingBoxIndicesPerMesh);

    //no diagonals, only change on one axis
    //back quad
    (*outIndices)[0] = l_l_b;
    (*outIndices)[1] = l_u_b;
    (*outIndices)[2] = l_l_b;
    (*outIndices)[3] = r_l_b;
    (*outIndices)[4] = r_u_b;
    (*outIndices)[5] = r_l_b;
    (*outIndices)[6] = l_u_b;
    (*outIndices)[7] = r_u_b;

    //front quad
    (*outIndices)[8]  = l_l_f;
    (*outIndices)[9]  = l_u_f;
    (*outIndices)[10] = l_l_f;
    (*outIndices)[11] = r_l_f;
    (*outIndices)[12] = r_u_f;
    (*outIndices)[13] = r_l_f;
    (*outIndices)[14] = l_u_f;
    (*outIndices)[15] = r_u_f;

    //top connections
    (*outIndices)[16]  = l_u_f;
    (*outIndices)[17]  = l_u_b;
    (*outIndices)[18] = r_u_f;
    (*outIndices)[19] = r_u_b;

    //bot connections
    (*outIndices)[20] = l_l_f;
    (*outIndices)[21] = l_l_b;
    (*outIndices)[22] = r_l_f;
    (*outIndices)[23] = r_l_b;
}

std::array<glm::vec3, 8> getAxisAlignedBoundingBoxPoints(const AxisAlignedBoundingBox& bb) {
    return {
        glm::vec3(bb.min.x, bb.min.y, bb.min.z),
        glm::vec3(bb.min.x, bb.min.y, bb.max.z),
        glm::vec3(bb.min.x, bb.max.y, bb.min.z),
        glm::vec3(bb.min.x, bb.max.y, bb.max.z),
        glm::vec3(bb.max.x, bb.min.y, bb.min.z),
        glm::vec3(bb.max.x, bb.min.y, bb.max.z),
        glm::vec3(bb.max.x, bb.max.y, bb.min.z),
        glm::vec3(bb.max.x, bb.max.y, bb.max.z)
    };
}

glm::mat4 viewProjectionMatrixAroundBB(const AxisAlignedBoundingBox& bb, const glm::vec3& viewDirection) {

    glm::mat4 coordinateSystemCorrection = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -0.5f, 0.f},
        {0.0f, 0.0f, 0.5f, 1.0f} };

    glm::mat4 V(1.f);
    glm::vec3 forward = viewDirection;
    glm::vec3 up = abs(forward.y) < 0.9999f ? glm::vec3(0.f, -1.f, 0.f) : glm::vec3(0.f, 0.f, -1.f);
    glm::vec3 right = cross(forward, up);
    up = cross(forward, right);
    V[0] = glm::vec4(normalize(right), 1.f);
    V[1] = glm::vec4(normalize(up), 1.f);
    V[2] = glm::vec4(forward, 1.f);    

    V = glm::transpose(V);

    //reference: https://developer.download.nvidia.com/SDK/10.5/opengl/src/cascaded_shadow_maps/doc/cascaded_shadow_maps.pdf
    const auto frustumPoints = getAxisAlignedBoundingBoxPoints(bb);
    glm::vec3 minP(std::numeric_limits<float>::max());
    glm::vec3 maxP(std::numeric_limits<float>::min());

    for (int i = 0; i < 8; i++) {
        glm::vec3 p = frustumPoints[i];
        glm::vec3 pTransformed = glm::mat3(V) * p;
        minP = min(minP, pTransformed);
        maxP = max(maxP, pTransformed);
    }

    glm::vec3 scale = glm::vec3(2.f) / (maxP - minP);
    glm::vec3 offset = -0.5f * (maxP + minP) * scale;

    glm::mat4 P(1.f);
    P[0][0] = scale.x;
    P[1][1] = scale.y;
    P[2][2] = scale.z;
    P[3][0] = offset.x;
    P[3][1] = offset.y;
    P[3][2] = offset.z;

    return coordinateSystemCorrection * P * V;
}