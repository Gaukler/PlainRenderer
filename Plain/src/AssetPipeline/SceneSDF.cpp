#include "pch.h"
#include "SceneSDF.h"
#include "Utilities/DirectoryUtils.h"
#include "VolumeInfo.h"
#include "Utilities/MathUtils.h"
#include "Common/sdfUtilities.h"

//private function declarations
ImageDescription ComputeSceneSDFTexture(const std::vector<MeshData>& meshes, const std::vector<AxisAlignedBoundingBox>& AABBList);

bool isAxisSeparating(const glm::vec3& axis, const glm::vec3 bbHalfVector,
	const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2);

bool doTriangleAABBOverlap(const glm::vec3& bbCenter, const glm::vec3& bbExtends,
	const glm::vec3& v0In, const glm::vec3& v1In, const glm::vec3& v2In, const glm::vec3& N);

std::vector<uint8_t> computeSDF(const glm::uvec3& resolution,
	const std::vector<AxisAlignedBoundingBox>& AABBList, const std::vector<MeshData>& meshes);

//---- implementation ----

ImageDescription ComputeSceneSDFTexture(const std::vector<MeshData>& meshes, const std::vector<AxisAlignedBoundingBox>& AABBList) {
    const uint32_t sdfRes = 64;
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
//reference: "Fast 3D Triangle-Box Overlap Testing"
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
	const AxisAlignedBoundingBox sceneBBPadded = padSDFBoundingBox(sceneBB, resolution);

    const VolumeInfo sdfVolumeInfo = volumeInfoFromBoundingBox(sceneBBPadded);
    
    struct TriangleInfo {
		glm::vec3 v0;
		glm::vec3 v1;
		glm::vec3 v2;
		glm::vec3 N;
    };
	
	//build uniform grid
    const glm::ivec3 uniformGridResolution = glm::ivec3(32);
	const glm::vec3 uniformGridCellSize = glm::vec3(sdfVolumeInfo.extends) / glm::vec3(uniformGridResolution);

	const size_t uniformGridCellCount = uniformGridResolution.x * uniformGridResolution.y * uniformGridResolution.z;
    std::vector<std::vector<TriangleInfo>> uniformGrid(uniformGridCellCount); //for every cell contains list of triangles

	auto flattenGridIndex = [](const glm::ivec3& index3D, const glm::ivec3& resolution) {
		return index3D.x + index3D.y * resolution.x + index3D.z * resolution.x * resolution.y;
	};

	auto pointToCellIndex = [](const glm::vec3& p, const AxisAlignedBoundingBox& aabb, const glm::ivec3 resolution) {
		const glm::vec3 pRelative = p - aabb.min;							//range [0:max-min]
		glm::vec3 normalized = pRelative / (aabb.max - aabb.min);		//range [0:1]
		normalized = glm::clamp(normalized, glm::vec3(0.f), glm::vec3(0.999f)); //do not allow 1, otherwise it would not be floored and be exactly resoltuin, exceeding max index
		const glm::vec3 posInTexels = normalized * glm::vec3(resolution);	//range [0:cellRes]
		const glm::ivec3 index = glm::floor(posInTexels);
		return index;
	};

	auto volumeIndexToCellCenter = [](const glm::ivec3& index, const glm::ivec3& resolution, const VolumeInfo& volume) {
		const glm::vec3 indexNormalized = (glm::vec3(index) + 0.5f) / glm::vec3(resolution);	//range [0:1]
		const glm::vec3 indexNormShifted = indexNormalized - 0.5f;								//range [-0.5:0.5]
		const glm::vec3 cellCenter = indexNormShifted * glm::vec3(volume.extends) + glm::vec3(volume.offset);
		return cellCenter;
	};

	for (size_t meshIndex = 0; meshIndex < meshes.size(); meshIndex++) {
		const MeshData mesh = meshes[meshIndex];
		for (size_t i = 0; i < mesh.indices.size(); i += 3) {

			const size_t i0 = mesh.indices[i];
			const size_t i1 = mesh.indices[i + 1];
			const size_t i2 = mesh.indices[i + 2];

			TriangleInfo triangle;
			triangle.v0 = mesh.positions[i0];
			triangle.v1 = mesh.positions[i1];
			triangle.v2 = mesh.positions[i2];

			triangle.N = glm::normalize(cross(triangle.v0 - triangle.v1, triangle.v0 - triangle.v2));

			const glm::vec3 triangleMin = glm::min(glm::min(triangle.v0, triangle.v1), triangle.v2);
			const glm::vec3 triangleMax = glm::max(glm::max(triangle.v0, triangle.v1), triangle.v2);

			const glm::ivec3 minIndex = pointToCellIndex(triangleMin, sceneBBPadded, uniformGridResolution);
			const glm::ivec3 maxIndex = pointToCellIndex(triangleMax, sceneBBPadded, uniformGridResolution);

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
	}

	std::cout << "Built uniform grid\n";

	//calculate SDF
    const size_t pixelCount = resolution.x * resolution.y * resolution.z;
    const size_t bytePerPixel = 2; //float 16
    const size_t byteCount = pixelCount * bytePerPixel;
    std::vector<uint8_t> byteData(byteCount);

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
			//std::cout << y << std::endl;
            for (size_t x = 0; x < resolution.x; x++) {

				const size_t index = flattenGridIndex(glm::ivec3(x, y, z), glm::ivec3(resolution));
                const size_t byteIndex = index * bytePerPixel;

                //ray triangle intersection
                //scratch a pixel has a sign error in computation of t
                //reference: https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/ray-triangle-intersection-geometric-solution
                //reference: https://courses.cs.washington.edu/courses/csep557/10au/lectures/triangle_intersection.pdf
				const glm::vec3 rayOrigin = volumeIndexToCellCenter(glm::ivec3(x, y, z), resolution, sdfVolumeInfo);
                float closestHitTotal = std::numeric_limits<float>::max();

                const int sampleCount1D = 15;

                size_t backHitCounter = 0;
				//for every ray, parametrized by angles theta and phi
				for (int sampleIndexX = 0; sampleIndexX < sampleCount1D; sampleIndexX++) {
                    for (int sampleIndexY = 0; sampleIndexY < sampleCount1D; sampleIndexY++) {

						//sampleIndexX = 1;
						//sampleIndexY = 4;

						float sampleX = sampleIndexX / float(sampleCount1D - 1);			//in range [0:1]
						float sampleY = sampleIndexY / float(sampleCount1D - 1) * 2 - 1;	//in range [-1:1]

						const float phi = sampleX * 2.f * 3.1415f;
						const float theta = acosf(sampleY);

                        bool isBackfaceHit = false;

                        const glm::vec2 angles = glm::vec2(phi, theta) / 3.1415f * 180.f;
                        const glm::vec3 rayDirection = directionToVector(angles);
                        float rayClosestHit = std::numeric_limits<float>::max();

						bool rayIsInBoundingBox = true;

						//start index
						glm::ivec3 uniformGridIndex = pointToCellIndex(rayOrigin, sceneBBPadded, uniformGridResolution);
						
						glm::vec3 currentRayPosition = rayOrigin;

						//traverse uniform grid until hit or going out of bounding box
						while (rayIsInBoundingBox) {
							const size_t cellIndex = flattenGridIndex(uniformGridIndex, uniformGridResolution);

							const glm::vec3 cellMin = sceneBBPadded.min + glm::vec3(uniformGridIndex) / glm::vec3(uniformGridResolution) * glm::vec3(sdfVolumeInfo.extends);
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

								const float d0 = glm::dot(triangle.N, cross(edge0, C0));
								const float d1 = glm::dot(triangle.N, cross(edge1, C1));
								const float d2 = glm::dot(triangle.N, cross(edge2, C2));

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
								float distanceToNextCellIntersection = std::numeric_limits<float>::max();

								//check x,y,z
								int intersectedComponent;
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
				assert(closestHitTotal != std::numeric_limits<float>::max());	//indicates no hits

                //using sign heuristic from "Dynamic Occlusion with Signed Distance Fields", page 22
                //assuming negative sign when more than half rays hit backface
                const size_t hitsTotal = sampleCount1D * sampleCount1D;
                const float backHitPercentage = backHitCounter / (float)hitsTotal;
				//assert(backHitPercentage == 0 || backHitPercentage == 1);
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