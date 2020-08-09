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
    const std::vector<glm::vec3> bbPoints = {
        glm::vec3(bb.min.x, bb.min.y, bb.min.z),
        glm::vec3(bb.min.x, bb.min.y, bb.max.z),
        glm::vec3(bb.min.x, bb.max.y, bb.min.z),
        glm::vec3(bb.min.x, bb.max.y, bb.max.z),
        glm::vec3(bb.max.x, bb.min.y, bb.min.z),
        glm::vec3(bb.max.x, bb.min.y, bb.max.z),
        glm::vec3(bb.max.x, bb.max.y, bb.min.z),
        glm::vec3(bb.max.x, bb.max.y, bb.max.z)
    };

    //transform
    std::vector<glm::vec3> bbPointsTransformed(8);
    for (uint32_t i = 0; i < 8; i++) {
        const auto& p = bbPoints[i];
        bbPointsTransformed[i] = glm::vec3(m * glm::vec4(p, 1.f));
    }

    //compute new bb
    return axisAlignedBoundingBoxFromPositions(bbPointsTransformed);
}


std::vector<glm::vec3> axisAlignedBoundingBoxToLineStrip(const AxisAlignedBoundingBox& bb) {
    std::vector<glm::vec3> vertices(axisAlignedBoundingBoxVerticesPerMesh);
    //make sure that there are no diagonals -> between points only one component(xyz) may change between min/max
    //first quad
    vertices[0] = glm::vec3(bb.min.x, bb.min.y, bb.min.z);
    vertices[1] = glm::vec3(bb.min.x, bb.min.y, bb.max.z);
    vertices[2] = glm::vec3(bb.min.x, bb.max.y, bb.max.z);
    vertices[3] = glm::vec3(bb.min.x, bb.max.y, bb.min.z);
    vertices[4] = glm::vec3(bb.min.x, bb.min.y, bb.min.z);
    
    //second quad, same as first but with max in x coordinate
    vertices[5] = glm::vec3(bb.max.x, bb.min.y, bb.min.z);
    vertices[6] = glm::vec3(bb.max.x, bb.min.y, bb.max.z);
    vertices[7] = glm::vec3(bb.max.x, bb.max.y, bb.max.z);
    vertices[8] = glm::vec3(bb.max.x, bb.max.y, bb.min.z);
    vertices[9] = glm::vec3(bb.max.x, bb.min.y, bb.min.z);

    //third quad
    vertices[10] = glm::vec3(bb.max.x, bb.min.y, bb.min.z);
    vertices[11] = glm::vec3(bb.min.x, bb.min.y, bb.min.z);
    vertices[12] = glm::vec3(bb.min.x, bb.max.y, bb.min.z);
    vertices[13] = glm::vec3(bb.max.x, bb.max.y, bb.min.z);
    vertices[14] = glm::vec3(bb.max.x, bb.min.y, bb.min.z);

    //fourth quad, same as third but with max in z coordinate
    vertices[15] = glm::vec3(bb.max.x, bb.min.y, bb.max.z);
    vertices[16] = glm::vec3(bb.min.x, bb.min.y, bb.max.z);
    vertices[17] = glm::vec3(bb.min.x, bb.max.y, bb.max.z);
    vertices[18] = glm::vec3(bb.max.x, bb.max.y, bb.max.z);
    vertices[19] = glm::vec3(bb.max.x, bb.min.y, bb.max.z);

    return vertices;
}