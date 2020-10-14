#include "pch.h"
#include "Culling.h"

//reference: http://www.lighthouse3d.com/tutorials/view-frustum-culling/
bool isAxisAlignedBoundingBoxIntersectingViewFrustum(const ViewFrustum& frustum, const AxisAlignedBoundingBox& bb) {

    //computes the sign which indicates on which side of the plane a point lies(+before plane, -behind plane, 0 on plane)
    //p: point to test, pPlane: point on plane, nPlane normal of plane 
    const auto pointPlaneHalfspaceSign = [](const glm::vec3 p, const glm::vec3 pPlane, const glm::vec3 nPlane) {
        return glm::sign(dot(p - pPlane, nPlane));
    };

    const auto& fps = frustum.points;
    const auto& fns = frustum.normals;

    //iterate over all bounding box points and check halfspace sign of every point defined by the plane
    glm::vec3 planePointNormalPairs[6][2] = {
        {fps.l_u_f, fns.top},
        {fps.l_l_f, fns.bot},
        {fps.l_u_n, fns.near},
        {fps.l_u_f, fns.far},
        {fps.l_u_f, fns.left},
        {fps.r_u_f, fns.right}
    };

    //iterate planes
    for (uint32_t i = 0; i < 6; i++) {
        const auto& pointNormalPair = planePointNormalPairs[i];
        const auto& planePoint  = pointNormalPair[0];
        const auto& planeNormal = pointNormalPair[1];

        bool isBBOutsidePlane = true;
        //iterate bounding box
        for (const auto& bp : getAxisAlignedBoundingBoxPoints(bb)) {
            isBBOutsidePlane &= pointPlaneHalfspaceSign(bp, planePoint, planeNormal) > 0;
        }
        if (isBBOutsidePlane) {
            return false;
        }
    }
    return true;
}