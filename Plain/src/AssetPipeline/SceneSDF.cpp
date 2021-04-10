#include "pch.h"
#include "SceneSDF.h"
#include "Utilities/DirectoryUtils.h"
#include "VolumeInfo.h"
#include "Utilities/MathUtils.h"
#include "Common/sdfUtilities.h"
#include "Common/JobSystem.h"

// ---- private function declarations ----

bool isAxisSeparating(const glm::vec3& axis, const glm::vec3 bbHalfVector,
    const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);

bool doTriangleAABBOverlap(const glm::vec3& bbCenter, const glm::vec3& bbExtends,
    const glm::vec3& v0In, const glm::vec3& v1In, const glm::vec3& v2In, const glm::vec3& N);

std::vector<uint8_t> computeSDF(const glm::uvec3& resolution, const AxisAlignedBoundingBox& aabb, const MeshData& mesh);

int flattenGridIndex(const glm::ivec3& index3D, const glm::ivec3& resolution);
glm::uvec3 pointToCellIndex(const glm::vec3& p, const AxisAlignedBoundingBox& aabb, const glm::ivec3 resolution);
glm::vec3 volumeIndexToCellCenter(const glm::ivec3& index, const glm::ivec3& resolution, const VolumeInfo& volume);

//the uniform grid contains triangle info directly instead of indices
//this increases memory consumption but improves speed due to better cache coherence
struct TriangleInfo {
    glm::vec3 v0;
    glm::vec3 v1;
    glm::vec3 v2;
    glm::vec3 N;
};

//uniform grid is used as acceleration structure for raytracing of SDF creation
std::vector<std::vector<TriangleInfo>> buildUniformGrid(const MeshData& mesh, const VolumeInfo& sdfVolumeInfo,
    const AxisAlignedBoundingBox& AABB, const glm::ivec3& uniformGridResolution);

uint32_t nextPowerOfTwo(const uint32_t in);
float computePointTrianglesClosestDistance(const glm::vec3 p, const std::vector<TriangleInfo>& triangles);

// ---- implementation ----

//see: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
uint32_t nextPowerOfTwo(const uint32_t in) {
    uint32_t out = in;
    out--;
    out |= out >> 1;
    out |= out >> 2;
    out |= out >> 4;
    out |= out >> 8;
    out |= out >> 16;
    out++;
    return out;
}

//reference: https://www.iquilezles.org/www/articles/triangledistance/triangledistance.htm
float computePointTrianglesClosestDistance(const glm::vec3 p, const std::vector<TriangleInfo>& triangles) {

    float closestD = std::numeric_limits<float>::infinity();
    for (const TriangleInfo& t : triangles) {

        const glm::vec3 v1ToP = p - t.v0;
        const glm::vec3 v2ToP = p - t.v1;
        const glm::vec3 v3ToP = p - t.v2;

        const glm::vec3 v0ToV1 = t.v1 - t.v0;
        const glm::vec3 v1ToV2 = t.v2 - t.v1;
        const glm::vec3 v2ToV0 = t.v0 - t.v2;

        const glm::vec3 eN0 = glm::cross(v0ToV1, t.N);
        const glm::vec3 eN1 = glm::cross(v1ToV2, t.N);
        const glm::vec3 eN2 = glm::cross(v2ToV0, t.N);

        const float s1 = glm::sign(glm::dot(eN0, -v1ToP));
        const float s2 = glm::sign(glm::dot(eN1, -v2ToP));
        const float s3 = glm::sign(glm::dot(eN2, -v3ToP));

        const bool onEdge =
            s1 +
            s2 +
            s3 < 2.f;

        const float c1 = glm::clamp(glm::dot(v1ToP, v0ToV1) / dot2(v0ToV1), 0.f, 1.f);
        const float c2 = glm::clamp(glm::dot(v2ToP, v1ToV2) / dot2(v1ToV2), 0.f, 1.f);
        const float c3 = glm::clamp(glm::dot(v3ToP, v2ToV0) / dot2(v2ToV0), 0.f, 1.f);

        float l1 = dot2(p - (t.v0 + v0ToV1 * c1));
        float l2 = dot2(p - (t.v1 + v1ToV2 * c2));
        float l3 = dot2(p - (t.v2 + v2ToV0 * c3));

        float d = onEdge ? glm::min(glm::min(l1, l2), l3) : abs(glm::dot(t.N, v1ToP) * glm::dot(t.N, v1ToP));
        d = abs(d);

        closestD = glm::min(closestD, d);
    }
    return sqrt(abs(closestD));
}

SceneSDFTextures computeSceneSDFTextures(const std::vector<MeshData>& meshes, const std::vector<AxisAlignedBoundingBox>& AABBList) {

    SceneSDFTextures result;
    result.descriptions.resize(meshes.size());
    result.data.resize(meshes.size());

    JobSystem::Counter workFinised;
    const std::chrono::system_clock::time_point startTime = std::chrono::system_clock::now();

    assert(meshes.size() == AABBList.size());
    for (size_t i = 0; i < meshes.size(); i++) {

        //disable workerIndex unused parameter warning
        #pragma warning( push )
        #pragma warning( disable : 4100)

        if (meshes[i].texturePaths.sdfTexturePath.empty()) {
            continue;
        }
        JobSystem::addJob([&result, &meshes, &AABBList, i](int workerIndex) {
            const MeshData& mesh = meshes[i];
            const AxisAlignedBoundingBox meshBB = AABBList[i];

            const uint32_t maxSdfRes = 64;
            const uint32_t minSdfRes = 16;
            const float targetTexelPerMeter = 0.25f;

            glm::uvec3 sdfRes;
            const glm::vec3 bbExtents = meshBB.max - meshBB.min;
            for (int component = 0; component < 3; component++) {
                const float targetRes = bbExtents[component] / targetTexelPerMeter;
            
                sdfRes[component] = nextPowerOfTwo((uint32_t)targetRes);
                sdfRes[component] = glm::clamp(sdfRes[component], minSdfRes, maxSdfRes);
            }

            ImageDescription sdfTexture;
            sdfTexture.width = sdfRes[0];
            sdfTexture.height = sdfRes[1];
            sdfTexture.depth = sdfRes[2];
            sdfTexture.type = ImageType::Type3D;
            sdfTexture.format = ImageFormat::R16_sFloat;
            sdfTexture.usageFlags = ImageUsageFlags::Storage | ImageUsageFlags::Sampled;
            sdfTexture.mipCount = MipCount::One;
            sdfTexture.autoCreateMips = false;

            result.descriptions[i] = sdfTexture;
            result.data[i] = computeSDF(glm::uvec3(sdfTexture.width, sdfTexture.height, sdfTexture.depth), meshBB, mesh);
        }, &workFinised);

        //reenable warning
        #pragma warning( pop )
    }

    JobSystem::waitOnCounter(workFinised);

    const std::chrono::duration<double> computationTime = std::chrono::system_clock::now() - startTime;
    std::cout << "SDF computation time: " << computationTime.count() << "s\n";

    return result;
}

//helper function for doTriangleAABBOverlap
bool isAxisSeparating(const glm::vec3& axis, const glm::vec3 bbHalfVector,
    const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2) {

    const float p0 = glm::dot(axis, v0);
    const float p1 = glm::dot(axis, v1);
    const float p2 = glm::dot(axis, v2);

    const float r = glm::dot(glm::abs(axis), bbHalfVector);

    const float pMin = glm::min(glm::min(p0, p1), p2);
    const float pMax = glm::max(glm::max(p0, p1), p2);

    const bool isSeparating = pMin > r || pMax < -r;
    return isSeparating;
}

//reference: https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/aabb-triangle.html
//reference: "Fast 3D Triangle-Box Overlap Testing"
//not entirely optimized, tests can be simplified depending on axis 
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
        isAxisSeparating(glm::cross(bbN0, e0), bbHalf, v0, v1, v2) ||
        isAxisSeparating(glm::cross(bbN1, e0), bbHalf, v0, v1, v2) ||
        isAxisSeparating(glm::cross(bbN2, e0), bbHalf, v0, v1, v2) ||
        //edge 1
        isAxisSeparating(glm::cross(bbN0, e1), bbHalf, v0, v1, v2) ||
        isAxisSeparating(glm::cross(bbN1, e1), bbHalf, v0, v1, v2) ||
        isAxisSeparating(glm::cross(bbN2, e1), bbHalf, v0, v1, v2) ||
        //edge 2
        isAxisSeparating(glm::cross(bbN0, e2), bbHalf, v0, v1, v2) ||
        isAxisSeparating(glm::cross(bbN1, e2), bbHalf, v0, v1, v2) ||
        isAxisSeparating(glm::cross(bbN2, e2), bbHalf, v0, v1, v2)) {
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

int flattenGridIndex(const glm::ivec3& index3D, const glm::ivec3& resolution) {
    return index3D.x + index3D.y * resolution.x + index3D.z * resolution.x * resolution.y;
};

glm::uvec3 pointToCellIndex(const glm::vec3& p, const AxisAlignedBoundingBox& aabb, const glm::ivec3 resolution) {
    const glm::vec3 pRelative = p - aabb.min;							//range [0:max-min]
    glm::vec3 normalized = pRelative / (aabb.max - aabb.min);		//range [0:1]
    normalized = glm::clamp(normalized, glm::vec3(0.f), glm::vec3(0.999f)); //do not allow 1, otherwise it would not be floored and be exactly resoltuin, exceeding max index
    const glm::vec3 posInTexels = normalized * glm::vec3(resolution);	//range [0:cellRes]
    const glm::ivec3 index = glm::floor(posInTexels);
    return index;
};

glm::vec3 volumeIndexToCellCenter(const glm::ivec3& index, const glm::ivec3& resolution, const VolumeInfo& volume) {
    const glm::vec3 indexNormalized = (glm::vec3(index) + 0.5f) / glm::vec3(resolution);	//range [0:1]
    const glm::vec3 indexNormShifted = indexNormalized - 0.5f;								//range [-0.5:0.5]
    const glm::vec3 cellCenter = indexNormShifted * glm::vec3(volume.extends) + glm::vec3(volume.offset);
    return cellCenter;
};

std::vector<std::vector<TriangleInfo>> buildUniformGrid(const MeshData& mesh, const VolumeInfo& sdfVolumeInfo,
    const AxisAlignedBoundingBox& AABB, const glm::ivec3& uniformGridResolution) {

    const glm::vec3 uniformGridCellSize = glm::vec3(sdfVolumeInfo.extends) / glm::vec3(uniformGridResolution);

    const uint32_t uniformGridCellCount = uniformGridResolution.x * uniformGridResolution.y * uniformGridResolution.z;
    std::vector<std::vector<TriangleInfo>> uniformGrid(uniformGridCellCount); //for every cell contains list of triangles

    for (size_t i = 0; i < mesh.indices.size(); i += 3) {

        const size_t i0 = mesh.indices[i];
        const size_t i1 = mesh.indices[i + 1];
        const size_t i2 = mesh.indices[i + 2];

        TriangleInfo triangle;
        triangle.v0 = mesh.positions[i0];
        triangle.v1 = mesh.positions[i1];
        triangle.v2 = mesh.positions[i2];

        triangle.N = glm::normalize(cross(triangle.v0 - triangle.v2, triangle.v0 - triangle.v1));

        const glm::vec3 triangleMin = glm::min(glm::min(triangle.v0, triangle.v1), triangle.v2);
        const glm::vec3 triangleMax = glm::max(glm::max(triangle.v0, triangle.v1), triangle.v2);

        const glm::ivec3 minIndex = pointToCellIndex(triangleMin, AABB, uniformGridResolution);
        const glm::ivec3 maxIndex = pointToCellIndex(triangleMax, AABB, uniformGridResolution);

        //iterate over cells within triangles bounding box
        for (int x = minIndex.x; x <= maxIndex.x; x++) {
            for (int y = minIndex.y; y <= maxIndex.y; y++) {
                for (int z = minIndex.z; z <= maxIndex.z; z++) {
                    const glm::vec3 cellCenter = volumeIndexToCellCenter(glm::ivec3(x, y, z), uniformGridResolution, sdfVolumeInfo);
                    if (doTriangleAABBOverlap(cellCenter, uniformGridCellSize, triangle.v0, triangle.v1, triangle.v2, triangle.N)) {
                        const size_t cellIndex = flattenGridIndex(glm::ivec3(x, y, z), uniformGridResolution);
                        uniformGrid[cellIndex].push_back(triangle);
                    }
                }
            }
        }
    }
    return uniformGrid;
}

std::vector<uint8_t> computeSDF(const glm::uvec3& resolution, const AxisAlignedBoundingBox& aabb, const MeshData& mesh) {

    const AxisAlignedBoundingBox AABBPadded = padSDFBoundingBox(aabb);
    const VolumeInfo sdfVolumeInfo = volumeInfoFromBoundingBox(AABBPadded);

    //build uniform grid
    const glm::uvec3 uniformGridResolution = glm::ivec3(16);
    const std::vector<std::vector<TriangleInfo>> uniformGrid = buildUniformGrid(mesh, sdfVolumeInfo, AABBPadded, uniformGridResolution);

    //if no rays hit, distance to closest triangle is computed
    //for this a vector of all triangles is prepared
    std::vector<TriangleInfo> totalMeshTriangles;
    totalMeshTriangles.reserve(mesh.indices.size() / 3);
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        TriangleInfo t;
        const uint32_t i0 = mesh.indices[i];
        const uint32_t i1 = mesh.indices[i+1];
        const uint32_t i2 = mesh.indices[i+2];
        t.v0 = mesh.positions[i0];
        t.v1 = mesh.positions[i1];
        t.v2 = mesh.positions[i2];
        t.N = glm::normalize(glm::cross(t.v0 - t.v2, t.v0 - t.v1));

        totalMeshTriangles.push_back(t);
    }

const glm::vec3 uniformGridCellSize = glm::vec3(sdfVolumeInfo.extends) / glm::vec3(uniformGridResolution);

    //calculate SDF
   const uint32_t pixelCount = resolution.x * resolution.y * resolution.z;
   const uint32_t bytePerPixel = 2; //distance stored as 16 bit float
   const uint32_t byteCount = pixelCount * bytePerPixel;
   std::vector<uint8_t> byteData(byteCount);

    //for every texel
    for (uint32_t z = 0; z < resolution.z; z++) {
        for (uint32_t y = 0; y < resolution.y; y++) {
            for (uint32_t x = 0; x < resolution.x; x++) {

                const uint32_t index = flattenGridIndex(glm::ivec3(x, y, z), glm::ivec3(resolution));
                const uint32_t byteIndex = index * bytePerPixel;

                //ray triangle intersection
                //scratch a pixel has a sign error in computation of t
                //reference: https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/ray-triangle-intersection-geometric-solution
                //reference: https://courses.cs.washington.edu/courses/csep557/10au/lectures/triangle_intersection.pdf
                const glm::vec3 rayOrigin = volumeIndexToCellCenter(glm::ivec3(x, y, z), resolution, sdfVolumeInfo);
                float closestHitTotal = std::numeric_limits<float>::infinity();

                const int sampleCount1D = 15;

                uint32_t backHitCounter = 0;
                //for every ray, parametrized by angles theta and phi
                for (int sampleIndexX = 0; sampleIndexX < sampleCount1D; sampleIndexX++) {
                    for (int sampleIndexY = 0; sampleIndexY < sampleCount1D; sampleIndexY++) {

                        float sampleX = sampleIndexX / float(sampleCount1D - 1);            //in range [0:1]
                        float sampleY = sampleIndexY / float(sampleCount1D - 1) * 2 - 1;    //in range [-1:1]

                        const float phi = sampleX * 2.f * 3.1415f;
                        const float theta = acosf(sampleY);
    
                        bool isBackfaceHit = false;

                        const glm::vec2 angles = glm::vec2(phi, theta) / 3.1415f * 180.f;
                        const glm::vec3 rayDirection = directionToVector(angles);
                        float rayClosestHit = std::numeric_limits<float>::infinity();

                        bool rayIsInBoundingBox = true;

                        //start index
                        glm::uvec3 uniformGridIndex = pointToCellIndex(rayOrigin, AABBPadded, uniformGridResolution);

                        glm::vec3 currentRayPosition = rayOrigin;

                        //traverse uniform grid until hit or going out of bounding box
                        while (rayIsInBoundingBox) {
                            const size_t cellIndex = flattenGridIndex(uniformGridIndex, uniformGridResolution);

                            const glm::vec3 cellMin = AABBPadded.min + glm::vec3(uniformGridIndex) / glm::vec3(uniformGridResolution) * glm::vec3(sdfVolumeInfo.extends);
                            const glm::vec3 cellMax = cellMin + uniformGridCellSize;

                            bool hitTriangle = false;
                            //for every triangle in uniform grid cell
                            for (const TriangleInfo& triangle : uniformGrid[cellIndex]) {

                                const float NoR = glm::dot(triangle.N, rayDirection);

                                if (abs(NoR) < 0.0001f) {
                                    continue; //ray parallel to triangle
                                }

                                const float D = glm::dot(triangle.N, triangle.v0);

                                const float t = (D - glm::dot(triangle.N, rayOrigin)) / NoR;

                                if (t < 0.f) {
                                    continue; //intersection in wrong direction
                                }

                                const glm::vec3 edge0 = triangle.v1 - triangle.v0;
                                const glm::vec3 edge1 = triangle.v2 - triangle.v1;
                                const glm::vec3 edge2 = triangle.v0 - triangle.v2;

                                const glm::vec3 planeIntersection = rayOrigin + rayDirection * t;

                                const glm::vec3 C0 = planeIntersection - triangle.v0;
                                const glm::vec3 C1 = planeIntersection - triangle.v1;
                                const glm::vec3 C2 = planeIntersection - triangle.v2;

                                const float d0 = glm::dot(triangle.N, cross(C0, edge0));
                                const float d1 = glm::dot(triangle.N, cross(C1, edge1));
                                const float d2 = glm::dot(triangle.N, cross(C2, edge2));

                                const bool isInsideTriangle =
                                    d0 >= 0.f &&
                                    d1 >= 0.f &&
                                    d2 >= 0.f;

                                if (!isInsideTriangle) {
                                    continue;
                                }

                                //discard hits in other cells
                                const glm::vec3 hitPos = rayOrigin + t * rayDirection;
                                const bool hitInCurrentCell = isPointInAABB(hitPos, cellMin, cellMax);

                                if (hitInCurrentCell) {
                                    hitTriangle = true;
                                }
                                else {
                                    continue;
                                }

                                //rayDirection is normalized so t is distance to hit
                                if (t < rayClosestHit) {
                                    rayClosestHit = t;
                                    const float test = glm::dot(rayDirection, triangle.N);
                                    isBackfaceHit = test > 0.f;
                                }
                            }

                            //stop if ray hit something in current cell
                            if (hitTriangle) {
                                break;
                            }
                            else {
                                //find next cell intersection
                                float distanceToNextCellIntersection = std::numeric_limits<float>::infinity();

                                //check x,y,z
                                int intersectedComponent = 0;
                                for (int component = 0; component < 3; component++) {
                                    if (rayDirection[component] == 0.f) {
                                        //parallel, ignore
                                        continue;
                                    }
                                    else  {
                                        float nextIntersection;
                                        if (rayDirection[component] > 0) {
                                            //moving in positive direction, intersecting with cell max
                                            nextIntersection = cellMax[component];
                                            //move to next cell if currently at intersection
                                            nextIntersection = nextIntersection == currentRayPosition[component]
                                                ? nextIntersection + uniformGridCellSize[component] : nextIntersection;
                                        }
                                        else {
                                            //moving in negative direction, intersecting with cell min
                                            nextIntersection = cellMin[component];
                                            //move to next cell if currently at intersection
                                            nextIntersection = nextIntersection == currentRayPosition[component]
                                                ? nextIntersection - uniformGridCellSize[component] : nextIntersection;
                                        }
                                        const float distanceToCellIntersection = (nextIntersection - currentRayPosition[component]) / rayDirection[component];
                                        //choose smallest distance
                                        if (distanceToCellIntersection < distanceToNextCellIntersection) {
                                            distanceToNextCellIntersection = distanceToCellIntersection;
                                            intersectedComponent = component;
                                        }
                                    }
                                }
                                assert(distanceToNextCellIntersection != 0);
                                currentRayPosition += distanceToNextCellIntersection * rayDirection;

                                //advance index
                                uniformGridIndex[intersectedComponent] += rayDirection[intersectedComponent] > 0 ? 1 : -1;

                                rayIsInBoundingBox = 
                                    uniformGridIndex[intersectedComponent] < uniformGridResolution[intersectedComponent] &&
                                    uniformGridIndex[intersectedComponent] >= 0;
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
                const size_t hitsTotal = sampleCount1D * sampleCount1D;
                const float backHitPercentage = backHitCounter / (float)hitsTotal;
                closestHitTotal *= backHitPercentage > 0.5f ? -1 : 1;

                if (closestHitTotal == std::numeric_limits<float>::infinity()) {
                    //indicates no hits, in this case assume that point is outside of mesh and compute distance to closest triangle
                    closestHitTotal = computePointTrianglesClosestDistance(rayOrigin, totalMeshTriangles);
                }

                uint16_t half = glm::packHalf(glm::vec1(closestHitTotal))[0];	//distance is stored as 16 bit float
                byteData[byteIndex] = ((uint8_t*)&half)[0];
                byteData[size_t(byteIndex) + 1] = ((uint8_t*)&half)[1];
            }
        }
    }

    return byteData;
}