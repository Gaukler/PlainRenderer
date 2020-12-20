#include "pch.h"
#include "SceneSDF.h"
#include "Utilities/DirectoryUtils.h"
#include "VolumeInfo.h"
#include "Utilities/MathUtils.h"

std::vector<uint8_t> computeSDF(const glm::uvec3& resolution,
    const std::vector<AxisAlignedBoundingBox>& AABBList, const std::vector<MeshData>& meshes);

ImageDescription ComputeSceneSDFTexture(const std::vector<MeshData>& meshes, const std::vector<AxisAlignedBoundingBox>& AABBList) {

    const uint32_t sdfRes = 8;
    ImageDescription desc;
    desc.width = sdfRes;
    desc.height = sdfRes;
    desc.depth = sdfRes;
    desc.type = ImageType::Type3D;
    desc.format = ImageFormat::R16_sFloat;
    desc.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
    desc.mipCount = MipCount::One;
    desc.autoCreateMips = false;
    desc.initialData = computeSDF(glm::uvec3(desc.width, desc.height, desc.depth), AABBList, meshes);
    return desc;
}

bool isAxisSeparating(const glm::vec3& axis, const glm::vec3 bbHalfVector, 
    const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2) {

    const float p0 = glm::dot(axis, v0);
    const float p1 = glm::dot(axis, v1);
    const float p2 = glm::dot(axis, v2);

    const float r = glm::abs(glm::dot(axis, bbHalfVector));

    const float pMin = glm::min(glm::min(p0, p1), p2);
    const float pMax = glm::max(glm::max(p0, p1), p2);

    return pMin > r || pMax < -r;
}

//reference: https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/aabb-triangle.html
//reference: "Fast 3D Triangle-Box OverlapTesting"
bool doTriangleAABBOverlap(const glm::vec3& bbCenter, const glm::vec3& bbExtends, 
    const glm::vec3& v0In, const glm::vec3& v1In, const glm::vec3& v2In, const glm::vec3& N) {

    //move triangle to world center
    const glm::vec3 v0 = v0In - bbCenter;
    const glm::vec3 v1 = v1In - bbCenter;
    const glm::vec3 v2 = v2In - bbCenter;

    //compute edges
    const glm::vec3 e0 = v1 - v0;
    const glm::vec3 e1 = v2 - v1;
    const glm::vec3 e2 = v0 - v2;

    const glm::vec3 bbHalf = bbExtends * 0.5f;

    //normals of bb are x,y,z -axis
    const glm::vec3 bbN0 = glm::vec3(1, 0, 0);
    const glm::vec3 bbN1 = glm::vec3(0, 1, 0);
    const glm::vec3 bbN2 = glm::vec3(0, 0, 1);

    //9-SAT tests
    if (
        //edge 0
        isAxisSeparating(glm::cross(e0, bbN0), bbHalf, v0, v1, v2) ||
        isAxisSeparating(glm::cross(e0, bbN1), bbHalf, v0, v1, v2) ||
        isAxisSeparating(glm::cross(e0, bbN2), bbHalf, v0, v1, v2) ||
        //edge 1
        isAxisSeparating(glm::cross(e1, bbN0), bbHalf, v0, v1, v2) ||
        isAxisSeparating(glm::cross(e1, bbN1), bbHalf, v0, v1, v2) ||
        isAxisSeparating(glm::cross(e1, bbN2), bbHalf, v0, v1, v2) ||
        //edge 2
        isAxisSeparating(glm::cross(e2, bbN0), bbHalf, v0, v1, v2) ||
        isAxisSeparating(glm::cross(e2, bbN1), bbHalf, v0, v1, v2) ||
        isAxisSeparating(glm::cross(e2, bbN2), bbHalf, v0, v1, v2)) {
        return false;
    }

    //test aabb normals
    if (
        isAxisSeparating(bbN0, bbHalf, v0, v1, v2) ||
        isAxisSeparating(bbN1, bbHalf, v0, v1, v2) ||
        isAxisSeparating(bbN2, bbHalf, v0, v1, v2)) {
        return false;
    }

    //test triangle normal
    if (isAxisSeparating(N, bbHalf, v0, v1, v2)) {
        return false;
    }

    //no separating axis found -> overlapping
    return true;
}

std::vector<uint8_t> computeSDF(const glm::uvec3& resolution,
    const std::vector<AxisAlignedBoundingBox>& AABBList, const std::vector<MeshData>& meshes) {

    auto dot2 = [](const glm::vec3& v) {
        return glm::dot(v, v);
    };

    const auto startTime = std::chrono::system_clock::now();

    const AxisAlignedBoundingBox sceneBB = combineAxisAlignedBoundingBoxes(AABBList);
    AxisAlignedBoundingBox sceneBBPadded = sceneBB;

    const float bbBias = 1.f;
    sceneBBPadded.max += bbBias;
    sceneBBPadded.min -= bbBias;

    const VolumeInfo sdfVolumeInfo = volumeInfoFromBoundingBox(sceneBBPadded);
    
    struct TriangleIndex {
        size_t mesh;
        size_t indexBuffer;
    };

    const size_t uniformGridResolution = 3;
    std::vector<std::vector<TriangleIndex>> uniformGrid; //for every cell contains list of triangle indices

    for (size_t z = 0; z < uniformGridResolution; z++) {
        for (size_t y = 0; y < uniformGridResolution; y++) {
            for (size_t x = 0; x < uniformGridResolution; x++) {
                const glm::vec3 cellCenter = ((glm::vec3(x, y, z) + 0.5f) / glm::vec3(uniformGridResolution) - 0.5f) * glm::vec3(sdfVolumeInfo.extends) + glm::vec3(sdfVolumeInfo.offset);
                const glm::vec3 cellExtends = glm::vec3(sdfVolumeInfo.extends) / float(uniformGridResolution);

                std::vector<TriangleIndex> cellIndices;

                for (size_t meshIndex = 0; meshIndex < meshes.size(); meshIndex++) {
                    const MeshData mesh = meshes[meshIndex];
                    for (size_t i = 0; i < mesh.indices.size(); i += 3) {

                        const size_t i0 = mesh.indices[i];
                        const size_t i1 = mesh.indices[i + 1];
                        const size_t i2 = mesh.indices[i + 2];

                        const glm::vec3 v0 = mesh.positions[i0];
                        const glm::vec3 v1 = mesh.positions[i1];
                        const glm::vec3 v2 = mesh.positions[i2];

                        const glm::vec3 n0 = mesh.normals[i0];
                        const glm::vec3 n1 = mesh.normals[i1];
                        const glm::vec3 n2 = mesh.normals[i2];

                        const glm::vec3 N = glm::normalize(n0 + n1 + n2);

                        if (doTriangleAABBOverlap(
                            cellCenter, cellExtends, v0, v1, v2, N)) {
                                TriangleIndex triangleIndex;
                                triangleIndex.mesh = meshIndex;
                                triangleIndex.indexBuffer = i;
                                cellIndices.push_back(triangleIndex);
                        }
                    }
                }
                uniformGrid.push_back(cellIndices);
            }
        }
    }

    const size_t pixelCount = resolution.x * resolution.y * resolution.z;
    const size_t bytePerPixel = 2; //float 16
    const size_t byteCount = pixelCount * bytePerPixel;
    std::vector<uint8_t> byteData(byteCount);

    for (size_t z = 0; z < resolution.z; z++) {
        for (size_t y = 0; y < resolution.y; y++) {
            for (size_t x = 0; x < resolution.x; x++) {
                const size_t index = x + y * resolution.x + z * resolution.x * resolution.y;
                const size_t byteIndex = index * bytePerPixel;

                //ray triangle intersection
                //scratch a pixel has a sign error in computation of t
                //reference: https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/ray-triangle-intersection-geometric-solution
                //reference: https://courses.cs.washington.edu/courses/csep557/10au/lectures/triangle_intersection.pdf
                const glm::vec3 rayOrigin = ((glm::vec3(x, y, z) + 0.5f) / glm::vec3(resolution) - 0.5f) * glm::vec3(sdfVolumeInfo.extends) + glm::vec3(sdfVolumeInfo.offset);
                float closestHitTotal = std::numeric_limits<float>::max();

                const int sampleCount = 20;
                const float phiMax = 3.1415f * 2.f;
                const float thetaMax = 3.1415f;

                size_t backHitCounter = 0;
                for (float theta = 0.f; theta < thetaMax; theta += thetaMax / sampleCount) {
                    for (float phi = 0.f; phi < phiMax; phi += phiMax / sampleCount) {
                        bool isBackfaceHit = false;

                        const glm::vec2 angles = glm::vec2(phi, theta) / 3.1415f * 180.f;
                        const glm::vec3 rayDirection = directionToVector(angles);
                        float rayClosestHit = std::numeric_limits<float>::max();
                        for (const MeshData mesh : meshes) {
                            for (size_t i = 0; i < mesh.indices.size(); i += 3) {

                                const size_t i0 = mesh.indices[i];
                                const size_t i1 = mesh.indices[i + 1];
                                const size_t i2 = mesh.indices[i + 2];

                                const glm::vec3 n1 = mesh.normals[i0];
                                const glm::vec3 n2 = mesh.normals[i1];
                                const glm::vec3 n3 = mesh.normals[i2];

                                const glm::vec3 N = glm::normalize(n1 + n2 + n3);
                                const float NoR = glm::dot(N, rayDirection);

                                if (abs(NoR) < 0.0001f) {
                                    continue; //ray parallel to triangle
                                }

                                const glm::vec3 v0 = mesh.positions[i0];
                                const glm::vec3 v1 = mesh.positions[i1];
                                const glm::vec3 v2 = mesh.positions[i2];

                                const float D = glm::dot(N, v0);

                                const float t = (D - glm::dot(N, rayOrigin)) / NoR;

                                if (t < 0.f) {
                                    continue; //intersection in wrong direction
                                }

                                const glm::vec3 edge0 = v1 - v0;
                                const glm::vec3 edge1 = v2 - v1;
                                const glm::vec3 edge2 = v0 - v2;

                                const glm::vec3 planeIntersection = rayOrigin + rayDirection * t;

                                const glm::vec3 C0 = planeIntersection - v0;
                                const glm::vec3 C1 = planeIntersection - v1;
                                const glm::vec3 C2 = planeIntersection - v2;

                                const float d0 = glm::dot(N, cross(edge0, C0));
                                const float d1 = glm::dot(N, cross(edge1, C1));
                                const float d2 = glm::dot(N, cross(edge2, C2));

                                bool isInsideTriangle =
                                    d0 >= 0.f &&
                                    d1 >= 0.f &&
                                    d2 >= 0.f;

                                if (!isInsideTriangle) {
                                    continue;
                                }

                                //rayDirection is normalized so t is distance to hit
                                if (t < rayClosestHit) {
                                    rayClosestHit = t;
                                    float test = glm::dot(rayDirection, N);
                                    isBackfaceHit = test > 0.f;
                                }
                            }
                        }
                        if (isBackfaceHit) {
                            backHitCounter++;
                        }
                        closestHitTotal = glm::min(closestHitTotal, rayClosestHit);
                    }
                }
                //using sign heuristic from "Dynamic Occlusion with Signed Distance Fields", page 22
                //assuming negative sign when more than half rays hit backface
                const size_t hitsTotal = sampleCount * sampleCount;
                float backHitPercentage = backHitCounter / (float)hitsTotal;
                closestHitTotal *= backHitPercentage > 0.5f ? -1 : 1;
                uint16_t half = glm::packHalf(glm::vec1(closestHitTotal))[0];
                byteData[byteIndex] = ((uint8_t*)&half)[0];
                byteData[byteIndex + 1] = ((uint8_t*)&half)[1];
            }
        }
    }

    const std::chrono::duration<double> computationTime = std::chrono::system_clock::now() - startTime;
    std::cout << "SDF computation time: " << computationTime.count() << "s\n";

    return byteData;
}