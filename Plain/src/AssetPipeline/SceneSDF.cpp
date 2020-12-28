#include "pch.h"
#include "SceneSDF.h"
#include "Utilities/DirectoryUtils.h"
#include "VolumeInfo.h"
#include "Utilities/MathUtils.h"

//private function declarations
void triangleAABBOverlapTests();

ImageDescription ComputeSceneSDFTexture(const std::vector<MeshData>& meshes, const std::vector<AxisAlignedBoundingBox>& AABBList);

bool isAxisSeparating(const glm::vec3& axis, const glm::vec3 bbHalfVector,
	const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);

bool doTriangleAABBOverlap(const glm::vec3& bbCenter, const glm::vec3& bbExtends,
	const glm::vec3& v0In, const glm::vec3& v1In, const glm::vec3& v2In, const glm::vec3& N);

std::vector<uint8_t> computeSDF(const glm::uvec3& resolution,
	const std::vector<AxisAlignedBoundingBox>& AABBList, const std::vector<MeshData>& meshes);

//---- implementation ----

void triangleAABBOverlapTests() {

	auto testPermutationVertexShuffle = [](
		const glm::vec3 bbCenter,
		const glm::vec3 bbExtends,
		const glm::vec3& v0,
		const glm::vec3& v1,
		const glm::vec3& v2,
		const glm::vec3& N,
		bool expectedResult){
		assert(doTriangleAABBOverlap(bbCenter, bbExtends, v0, v1, v2, N) == expectedResult);
		assert(doTriangleAABBOverlap(bbCenter, bbExtends, v0, v2, v1, N) == expectedResult);
		assert(doTriangleAABBOverlap(bbCenter, bbExtends, v1, v2, v0, N) == expectedResult);
		assert(doTriangleAABBOverlap(bbCenter, bbExtends, v1, v0, v2, N) == expectedResult);
		assert(doTriangleAABBOverlap(bbCenter, bbExtends, v2, v0, v1, N) == expectedResult);
		assert(doTriangleAABBOverlap(bbCenter, bbExtends, v2, v1, v0, N) == expectedResult);
	};

	auto testPermutationNormalFlip = [testPermutationVertexShuffle](
		const glm::vec3 bbCenter,
		const glm::vec3 bbExtends,
		const glm::vec3& v0,
		const glm::vec3& v1,
		const glm::vec3& v2,
		const glm::vec3& N,
		bool expectedResult) {
		testPermutationVertexShuffle(bbCenter, bbExtends, v0, v1, v2, N, expectedResult);
		testPermutationVertexShuffle(bbCenter, bbExtends, v0, v1, v2, -N, expectedResult);
	};

	auto testPermutationScale = [testPermutationNormalFlip](
		const glm::vec3 bbCenter,
		const glm::vec3 bbExtends,
		const glm::vec3& v0,
		const glm::vec3& v1,
		const glm::vec3& v2,
		const glm::vec3& N,
		bool expectedResult
		) {
		float scale = 0.01;
		testPermutationNormalFlip(bbCenter * scale, bbExtends * scale, v0 * scale, v1 * scale, v2 * scale, N, expectedResult);
		scale = 0.1;
		testPermutationNormalFlip(bbCenter * scale, bbExtends * scale, v0 * scale, v1 * scale, v2 * scale, N, expectedResult);
		scale = 0.5;
		testPermutationNormalFlip(bbCenter * scale, bbExtends * scale, v0 * scale, v1 * scale, v2 * scale, N, expectedResult);
		scale = 1.f;
		testPermutationNormalFlip(bbCenter * scale, bbExtends * scale, v0 * scale, v1 * scale, v2 * scale, N, expectedResult);
		scale = 2.f;
		testPermutationNormalFlip(bbCenter * scale, bbExtends * scale, v0 * scale, v1 * scale, v2 * scale, N, expectedResult);
		scale = 10.f;
		testPermutationNormalFlip(bbCenter * scale, bbExtends * scale, v0 * scale, v1 * scale, v2 * scale, N, expectedResult);
	};

	auto testPermutationOffset = [testPermutationScale](
		const glm::vec3 bbCenter,
		const glm::vec3 bbExtends,
		const glm::vec3& v0,
		const glm::vec3& v1,
		const glm::vec3& v2,
		const glm::vec3& N,
		bool expectedResult
		) {
		glm::vec3 offset = glm::vec3(0.f);
		testPermutationScale(bbCenter + offset, bbExtends, v0 + offset, v1 + offset, v2 + offset, N, expectedResult);
		offset = glm::vec3(1.f);
		testPermutationScale(bbCenter + offset, bbExtends, v0 + offset, v1 + offset, v2 + offset, N, expectedResult);
		offset = glm::vec3(-1.f);
		testPermutationScale(bbCenter + offset, bbExtends, v0 + offset, v1 + offset, v2 + offset, N, expectedResult);
		offset = glm::vec3(10.f);
		testPermutationScale(bbCenter + offset, bbExtends, v0 + offset, v1 + offset, v2 + offset, N, expectedResult);
		offset = glm::vec3(-10.f);
		testPermutationScale(bbCenter + offset, bbExtends, v0 + offset, v1 + offset, v2 + offset, N, expectedResult);
	};

	{
		const glm::vec3 bbCenter = glm::vec3(0.f);
		const glm::vec3 bbExtends = glm::vec3(2.f);

		glm::vec3 v0 = glm::vec3(0.5f, 0.f, 0.f);
		glm::vec3 v1 = glm::vec3(-0.5f, 0.f, 0.f);
		glm::vec3 v2 = glm::vec3(0.f, 0.5f, 0.f);

		const glm::vec3 N = glm::normalize(glm::cross(v0 - v1, v0 - v2));

		testPermutationOffset(bbCenter, bbExtends, v0, v1, v2, N, true);

		v2 = glm::vec3(0.f, 0.f, 0.5f);
		testPermutationOffset(bbCenter, bbExtends, v0, v1, v2, N, true);

		v0 = glm::vec3( 0.5f, 2.f, 0.f);
		v1 = glm::vec3(-0.5f, 2.f, 0.f);
		v2 = glm::vec3(0.f, 2.f, 1.f);
		testPermutationOffset(bbCenter, bbExtends, v0, v1, v2, N, false);

		v0 = glm::vec3(1.5f, -0.2f, 0.f);
		v1 = glm::vec3(-1.5f, 0.2f, 0.f);
		v2 = glm::vec3(0.f, 2.f, 1.f);
		testPermutationOffset(bbCenter, bbExtends, v0, v1, v2, N, true);
	}
}

ImageDescription ComputeSceneSDFTexture(const std::vector<MeshData>& meshes, const std::vector<AxisAlignedBoundingBox>& AABBList) {
	//triangleAABBOverlapTests();
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

	const float r = glm::dot(glm::abs(axis), bbHalfVector);

	const float pMin = glm::min(glm::min(p0, p1), p2);
	const float pMax = glm::max(glm::max(p0, p1), p2);

	const bool isSeparating = pMin > r || pMax < -r;
	return isSeparating;
}

//reference: https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/aabb-triangle.html
//reference: "	"
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
	
	//build uniform grid
    const size_t uniformGridResolution = 6;
	const glm::vec3 uniformGridCellSize = glm::vec3(sdfVolumeInfo.extends) / float(uniformGridResolution);
    std::vector<std::vector<TriangleIndex>> uniformGrid; //for every cell contains list of triangle indices

    for (size_t z = 0; z < uniformGridResolution; z++) {
        for (size_t y = 0; y < uniformGridResolution; y++) {
            for (size_t x = 0; x < uniformGridResolution; x++) {
                const glm::vec3 cellCenter = ((glm::vec3(x, y, z) + 0.5f) / glm::vec3(uniformGridResolution) - 0.5f) * glm::vec3(sdfVolumeInfo.extends) + glm::vec3(sdfVolumeInfo.offset);

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

						const bool isTriangleInCell = doTriangleAABBOverlap(
							cellCenter, uniformGridCellSize, v0, v1, v2, N)
							
							//|| true//FOR TESTING
							;	

                        if (isTriangleInCell) {
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

	//calculate SDF
    const size_t pixelCount = resolution.x * resolution.y * resolution.z;
    const size_t bytePerPixel = 2; //float 16
    const size_t byteCount = pixelCount * bytePerPixel;
    std::vector<uint8_t> byteData(byteCount);

	auto flattenGridIndex = [](const glm::ivec3& index3D, const glm::ivec3& resolution) {
		return index3D.x + index3D.y * resolution.x + index3D.z * resolution.x * resolution.y;
	};

	auto isPointInAABB = [](const glm::vec3 p, const glm::vec3 min, const glm::vec3 max) {
		return 
			p.x <= max.x &&
			p.x >= min.x &&
			p.y <= max.y &&
			p.y >= min.y &&
			p.z <= max.z &&
			p.z >= min.z;
	};

	//for every texel
    for (size_t z = 0; z < resolution.z; z++) {
		std::cout << z << std::endl;
        for (size_t y = 0; y < resolution.y; y++) {
            for (size_t x = 0; x < resolution.x; x++) {

				const size_t index = flattenGridIndex(glm::ivec3(x, y, z), glm::ivec3(resolution));
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
				//for every ray, parametrized by angles theta and phi
                for (float theta = 0.f; theta < thetaMax; theta += thetaMax / sampleCount) {
                    for (float phi = 0.f; phi < phiMax; phi += phiMax / sampleCount) {
                        bool isBackfaceHit = false;

                        const glm::vec2 angles = glm::vec2(phi, theta) / 3.1415f * 180.f;
                        const glm::vec3 rayDirection = directionToVector(angles);
                        float rayClosestHit = std::numeric_limits<float>::max();

						bool rayIsInBoundingBox = true;

						//start index
						const glm::vec3 posRelativeToVolume = rayOrigin - sceneBBPadded.min;				//range [0:max-min]
						const glm::vec3 posNormalized = posRelativeToVolume / glm::vec3(sdfVolumeInfo.extends); //range [0:1]
						const glm::vec3 posInTexels = posNormalized * glm::vec3(uniformGridResolution);			//range [0:cellRes]
						glm::ivec3 uniformGridIndex = glm::floor(posInTexels);

						glm::vec3 rayGridTraversalFraction(0.5f); //starts in center of cell, cell border is hit at 0 and 1
						
						//traverse uniform grid until hit or going out of bounding box
						while (rayIsInBoundingBox) {
							const size_t cellIndex = flattenGridIndex(uniformGridIndex, glm::ivec3(uniformGridResolution));

							bool hitTriangle = false;
							//for every triangle in uniform grid cell
							for (const TriangleIndex& triangleIndex : uniformGrid[cellIndex]) {

								const MeshData mesh = meshes[triangleIndex.mesh];

								const size_t i0 = mesh.indices[triangleIndex.indexBuffer];
								const size_t i1 = mesh.indices[triangleIndex.indexBuffer + 1];
								const size_t i2 = mesh.indices[triangleIndex.indexBuffer + 2];

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

								const bool isInsideTriangle =
									d0 >= 0.f &&
									d1 >= 0.f &&
									d2 >= 0.f;

								if (!isInsideTriangle) {
									continue;
								}

								//discard hits in other cells
								const glm::vec3 hitPos = rayOrigin + t * rayDirection;
								const glm::vec3 cellMin = sceneBBPadded.min + glm::vec3(uniformGridIndex) / glm::vec3(uniformGridResolution) * glm::vec3(sdfVolumeInfo.extends);
								const glm::vec3 cellMax = cellMin + uniformGridCellSize;
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
									const float test = glm::dot(rayDirection, N);
									isBackfaceHit = test > 0.f;
								}
							}

							//stop if ray hit something in current cell
							if (hitTriangle) {
								break;
							}
							else {
								//find next cell intersection
								float distanceToNextCellIntersection = std::numeric_limits<float>::max();

								//check x,y,z
								int intersectedComponent;
								for (int component = 0; component < 3; component++) {
									const float fractionChange = rayDirection[component] / uniformGridCellSize[component];
									float distanceToCellIntersection;
									if (abs(fractionChange) < 0.0001f) {
										//parallel, ignore
										continue;
									}
									else if (fractionChange > 0) {
										//going positive solve 1 = fractionCurrent + fractionChange*t
										//t = (1 - fractionCurrent) / fractionChange
										distanceToCellIntersection = (1 - rayGridTraversalFraction[component]) / fractionChange;
									}
									else {
										//going negative solve 0 = fractionCurrent + fractionChange*t
										//t = -fractionCurrent / fractionChange
										distanceToCellIntersection = -rayGridTraversalFraction[component] / fractionChange;
									}
									//choose smallest distance
									if (distanceToCellIntersection < distanceToNextCellIntersection) {
										distanceToNextCellIntersection = distanceToCellIntersection;
										intersectedComponent = component;
									}
								}
								//update fractions
								rayGridTraversalFraction += distanceToNextCellIntersection * rayDirection / uniformGridCellSize;

								//reset fraction that intersected
								rayGridTraversalFraction[intersectedComponent] = rayDirection[intersectedComponent] > 0 ? 0 : 1;

								//advance index
								uniformGridIndex[intersectedComponent] += rayDirection[intersectedComponent] > 0 ? 1 : -1;

								rayIsInBoundingBox = 
									uniformGridIndex[intersectedComponent] < uniformGridResolution &&
									uniformGridIndex[intersectedComponent] >= 0;
							}
						}

                        if (isBackfaceHit) {
                            backHitCounter++;
                        }
                        closestHitTotal = glm::min(closestHitTotal, rayClosestHit);
                    }
                }
				assert(closestHitTotal != std::numeric_limits<float>::max());	//indicates no hits

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