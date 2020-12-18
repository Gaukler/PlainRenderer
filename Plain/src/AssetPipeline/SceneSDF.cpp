#include "pch.h"
#include "SceneSDF.h"
#include "Utilities/DirectoryUtils.h"

std::vector<uint8_t> computeSDF(const glm::uvec3& resolution,
    const std::vector<AxisAlignedBoundingBox>& AABBList, const std::vector<MeshData>& meshes);

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

std::vector<uint8_t> computeSDF(const glm::uvec3& resolution,
    const std::vector<AxisAlignedBoundingBox>& AABBList, const std::vector<MeshData>& meshes) {

    auto dot2 = [](const glm::vec3& v) {
        return glm::dot(v, v);
    };

    const auto startTime = std::chrono::system_clock::now();

    auto sceneBB = combineAxisAlignedBoundingBoxes(AABBList);

    const float bbBias = 1.f;
    sceneBB.max += bbBias;
    sceneBB.min -= bbBias;

    glm::vec3 offset = glm::vec4((sceneBB.max + sceneBB.min) * 0.5f, 0.f);
    glm::vec3 extends = glm::vec4((sceneBB.max - sceneBB.min), 0.f);

    struct TriangleInfo {
        glm::vec3 v1;
        glm::vec3 v2;
        glm::vec3 v3;
        glm::vec3 eN1;
        glm::vec3 eN2;
        glm::vec3 eN3;
        glm::vec3 v1ToV2;
        glm::vec3 v2ToV3;
        glm::vec3 v3ToV1;
        glm::vec3 N;
        float v12DotInv;
        float v23DotInv;
        float v31DotInv;
    };

    std::vector<TriangleInfo> triangles;
    for (const auto& mesh : meshes) {
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            const uint32_t i1 = mesh.indices[i];
            const uint32_t i2 = mesh.indices[i + 1];
            const uint32_t i3 = mesh.indices[i + 2];

            TriangleInfo t;
            t.v1 = mesh.positions[i1];
            t.v2 = mesh.positions[i2];
            t.v3 = mesh.positions[i3];

            t.N = glm::normalize(mesh.normals[i1] + mesh.normals[i2] + mesh.normals[i3]);

            t.v1ToV2 = t.v2 - t.v1;
            t.v2ToV3 = t.v3 - t.v2;
            t.v3ToV1 = t.v1 - t.v3;

            t.eN1 = glm::cross(t.v1ToV2, t.N);
            t.eN2 = glm::cross(t.v2ToV3, t.N);
            t.eN3 = glm::cross(t.v3ToV1, t.N);

            t.v12DotInv = 1.f / glm::dot(t.v1ToV2, t.v1ToV2);
            t.v23DotInv = 1.f / glm::dot(t.v2ToV3, t.v2ToV3);
            t.v31DotInv = 1.f / glm::dot(t.v3ToV1, t.v3ToV1);

            triangles.push_back(t);
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

                //reference: https://www.iquilezles.org/www/articles/triangledistance/triangledistance.htm
                const glm::vec3 p = (glm::vec3(x, y, z) / glm::vec3(resolution) - 0.5f) * extends + offset;

                float value = std::numeric_limits<float>::max();
                for (const TriangleInfo& t : triangles) {

                    const glm::vec3 v1ToP = p - t.v1;
                    const glm::vec3 v2ToP = p - t.v2;
                    const glm::vec3 v3ToP = p - t.v3;

                    float c1 = glm::clamp(glm::dot(v1ToP, t.v1ToV2) * t.v12DotInv, 0.f, 1.f);
                    float c2 = glm::clamp(glm::dot(v2ToP, t.v2ToV3) * t.v23DotInv, 0.f, 1.f);
                    float c3 = glm::clamp(glm::dot(v3ToP, t.v3ToV1) * t.v31DotInv, 0.f, 1.f);

                    float s1 = glm::sign(glm::dot(t.eN1, -v1ToP));
                    float s2 = glm::sign(glm::dot(t.eN2, -v2ToP));
                    float s3 = glm::sign(glm::dot(t.eN3, -v3ToP));

                    bool onEdge =
                        s1 +
                        s2 +
                        s3 < 2.f;

                    float l1 = dot2(p - (t.v1 + t.v1ToV2 * c1));
                    float l2 = dot2(p - (t.v2 + t.v2ToV3 * c2));
                    float l3 = dot2(p - (t.v3 + t.v3ToV1 * c3));

                    float d = onEdge ? glm::min(glm::min(
                        l1,
                        l2),
                        l3)
                        :
                        abs(glm::dot(t.N, v1ToP) * glm::dot(t.N, v1ToP));

                    if (d < abs(value)) {
                        bool inFront = glm::dot(v1ToP, t.N) > 0;
                        d *= inFront ? 1 : -1;
                        value = d;
                    }
                }
                value = glm::sign(value) * sqrt(abs(value));
                uint16_t half = glm::packHalf(glm::vec1(value))[0];
                byteData[byteIndex] = ((uint8_t*)&half)[0];
                byteData[byteIndex + 1] = ((uint8_t*)&half)[1];
            }
        }
    }

    const std::chrono::duration<double> computationTime = std::chrono::system_clock::now() - startTime;
    std::cout << "SDF computation time: " << computationTime.count() << "s\n";

    return byteData;
}