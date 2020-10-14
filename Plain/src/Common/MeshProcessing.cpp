#include "pch.h"
#include "MeshProcessing.h"
#include "Common/TypeConversion.h"

void computeTangentBitangent(MeshData* outMeshData) {
    assert(outMeshData != nullptr);
    assert(outMeshData->positions.size() == outMeshData->uvs.size());
    assert(outMeshData->tangents.size() == 0);
    assert(outMeshData->bitangents.size() == 0);

    outMeshData->tangents.reserve(outMeshData->positions.size());
    outMeshData->bitangents.reserve(outMeshData->positions.size());

    for (uint32_t triangle = 0; triangle < outMeshData->positions.size() / 3; triangle++) {

        const uint32_t i0 = triangle * 3;
        const uint32_t i1 = triangle * 3 + 1;
        const uint32_t i2 = triangle * 3 + 2;

        const glm::vec3 v0 = outMeshData->positions[i0];
        const glm::vec3 v1 = outMeshData->positions[i1];
        const glm::vec3 v2 = outMeshData->positions[i2];

        glm::vec2 uv0 = outMeshData->uvs[i0];
        glm::vec2 uv1 = outMeshData->uvs[i1];
        glm::vec2 uv2 = outMeshData->uvs[i2];

        const glm::vec3 edge1 = v1 - v0;
        const glm::vec3 edge2 = v2 - v0;
        const glm::vec2 uvDelta1 = uv1 - uv0;
        const glm::vec2 uvDelta2 = uv2 - uv0;

        const float denom = 1.f / (uvDelta1.x * uvDelta2.y - uvDelta1.y * uvDelta2.x);
        const glm::vec3 tangent = denom * (uvDelta2.y * edge1 - uvDelta1.y * edge2);
        const glm::vec3 bitangent = denom * (uvDelta1.x * edge2 - uvDelta2.x * edge1);

        outMeshData->tangents.push_back(tangent);
        outMeshData->tangents.push_back(tangent);
        outMeshData->tangents.push_back(tangent);

        outMeshData->bitangents.push_back(bitangent);
        outMeshData->bitangents.push_back(bitangent);
        outMeshData->bitangents.push_back(bitangent);
    }
}

MeshData buildIndexedData(const MeshData& rawData) {

    assert(rawData.positions.size() == rawData.uvs.size());
    assert(rawData.positions.size() == rawData.normals.size());
    assert(rawData.positions.size() == rawData.tangents.size());
    assert(rawData.positions.size() == rawData.bitangents.size());

    auto findSimilar = [](const glm::vec3& positionVector, const glm::vec2& uv, const glm::vec3& normal, const MeshData& data, uint32_t* outSimilarIndex) {
        //iterate all indexed vertices
        for (uint32_t i = 0; i < data.positions.size(); i++) {

            glm::vec2 uvDiff = abs(data.uvs[i] - uv);
            if (uvDiff.x + uvDiff.y < 0.01f) {
                continue;
            }

            glm::vec3 posDiff = abs(data.positions[i] - positionVector);
            if (posDiff.x + posDiff.y + posDiff.z < 0.01f) {
                continue;
            }

            glm::vec3 normalDiff = abs(data.normals[i] - normal);
            if (normalDiff.x + normalDiff.y + normalDiff.z < 0.01f) {
                continue;
            }

            *outSimilarIndex = i;
        }
        return false;
    };


    //iterate over all vertices
    MeshData indexedData;
    uint32_t newIndex = 0;

    indexedData.indices.reserve(rawData.positions.size());
    indexedData.positions.reserve(rawData.positions.size());
    indexedData.normals.reserve(rawData.positions.size());
    indexedData.uvs.reserve(rawData.positions.size());
    indexedData.tangents.reserve(rawData.positions.size());
    indexedData.bitangents.reserve(rawData.positions.size());

    for (uint32_t indexToAdd = 0; indexToAdd < rawData.positions.size(); indexToAdd++) {

        uint32_t similarIndex;

        const glm::vec3 pos = rawData.positions[indexToAdd];
        const glm::vec2 uv = rawData.uvs[indexToAdd];
        const glm::vec3 normal = rawData.normals[indexToAdd];

        const bool alreadyExists = findSimilar(pos, uv, normal, indexedData, &similarIndex);
        if (alreadyExists) {
            indexedData.indices.push_back(similarIndex);
            indexedData.tangents[similarIndex] += rawData.tangents[indexToAdd];
            indexedData.bitangents[similarIndex] += rawData.bitangents[indexToAdd];
        }
        else {
            indexedData.positions.push_back(rawData.positions[indexToAdd]);
            indexedData.uvs.push_back(rawData.uvs[indexToAdd]);
            indexedData.normals.push_back(rawData.normals[indexToAdd]);
            indexedData.tangents.push_back(rawData.tangents[indexToAdd]);
            indexedData.bitangents.push_back(rawData.bitangents[indexToAdd]);
            indexedData.indices.push_back(newIndex);
            newIndex++;
        }
    }

    for (uint32_t i = 0; i < rawData.positions.size(); i++) {
        indexedData.tangents[i] = glm::normalize(indexedData.tangents[i]);
        indexedData.bitangents[i] = glm::normalize(indexedData.bitangents[i]);
    }

    return indexedData;
}

std::vector<MeshBinary> meshesToBinary(const std::vector<MeshData>& meshes) {

    std::vector<MeshBinary> meshesBinary;
    meshesBinary.reserve(meshes.size());

    for (const MeshData& meshData : meshes) {

        MeshBinary meshBinary;
        meshBinary.texturePaths = meshData.texturePaths;
        meshBinary.boundingBox = axisAlignedBoundingBoxFromPositions(meshData.positions);

        //index buffer
        meshBinary.indexCount = meshData.indices.size();
        if (meshBinary.indexCount < std::numeric_limits<uint16_t>::max()) {
            //half precision indices are enough
            //calculate lower precision indices
            meshBinary.indexBuffer.reserve(meshBinary.indexCount);
            for (const auto index : meshData.indices) {
                meshBinary.indexBuffer.push_back((uint16_t)index);
            }
        }
        else {
            //copy full precision indices
            const uint32_t entryPerIndex = 2; //two 16 bit entries needed for one 32 bit index
            meshBinary.indexBuffer.resize(meshBinary.indexCount * entryPerIndex);
            const size_t copySize = sizeof(uint32_t) * meshBinary.indexCount;
            memcpy(meshBinary.indexBuffer.data(), meshData.indices.data(), copySize);
        }

        //vertex buffer
        assert(meshData.positions.size() == meshData.uvs.size());
        assert(meshData.positions.size() == meshData.normals.size());
        assert(meshData.positions.size() == meshData.tangents.size());
        assert(meshData.positions.size() == meshData.bitangents.size());

        meshBinary.vertexCount = meshData.positions.size();

        //precision and type must correspond to types in VertexInput.h
        for (size_t i = 0; i < meshBinary.vertexCount; i++) {
            //position
            meshBinary.vertexBuffer.push_back(((uint8_t*)&meshData.positions[i].x)[0]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&meshData.positions[i].x)[1]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&meshData.positions[i].x)[2]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&meshData.positions[i].x)[3]);

            meshBinary.vertexBuffer.push_back(((uint8_t*)&meshData.positions[i].y)[0]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&meshData.positions[i].y)[1]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&meshData.positions[i].y)[2]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&meshData.positions[i].y)[3]);

            meshBinary.vertexBuffer.push_back(((uint8_t*)&meshData.positions[i].z)[0]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&meshData.positions[i].z)[1]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&meshData.positions[i].z)[2]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&meshData.positions[i].z)[3]);

            //uv stored as 16 bit signed float
            const auto uHalf = glm::packHalf(glm::vec1(meshData.uvs[i].x));
            meshBinary.vertexBuffer.push_back(((uint8_t*)&uHalf)[0]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&uHalf)[1]);

            const auto vHalf = glm::packHalf(glm::vec1(meshData.uvs[i].y));
            meshBinary.vertexBuffer.push_back(((uint8_t*)&vHalf)[0]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&vHalf)[1]);

            //normal stored as 32 bit R10G10B10A2
            const uint32_t normalR10G10B10A2 = vec3ToNormalizedR10B10G10A2(meshData.normals[i]);

            meshBinary.vertexBuffer.push_back(((uint8_t*)&normalR10G10B10A2)[0]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&normalR10G10B10A2)[1]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&normalR10G10B10A2)[2]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&normalR10G10B10A2)[3]);

            //tangent stored as 32 bit R10G10B10A2
            const uint32_t tangentR10G10B10A2 = vec3ToNormalizedR10B10G10A2(meshData.tangents[i]);

            meshBinary.vertexBuffer.push_back(((uint8_t*)&tangentR10G10B10A2)[0]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&tangentR10G10B10A2)[1]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&tangentR10G10B10A2)[2]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&tangentR10G10B10A2)[3]);

            //stored as 32 bit R10G10B10A2
            const uint32_t bitangentR10G10B10A2 = vec3ToNormalizedR10B10G10A2(meshData.tangents[i]);

            meshBinary.vertexBuffer.push_back(((uint8_t*)&bitangentR10G10B10A2)[0]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&bitangentR10G10B10A2)[1]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&bitangentR10G10B10A2)[2]);
            meshBinary.vertexBuffer.push_back(((uint8_t*)&bitangentR10G10B10A2)[3]);
        }
        meshesBinary.push_back(meshBinary);
    }

    return meshesBinary;
}