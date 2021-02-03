#include "pch.h"
#include "MeshProcessing.h"
#include "Common/CompressedTypes.h"

std::vector<AxisAlignedBoundingBox> AABBListFromMeshes(const std::vector<MeshData>& meshes) {
    std::vector<AxisAlignedBoundingBox> AABBList;
    AABBList.reserve(meshes.size());
    for (const MeshData& meshData : meshes) {
        AABBList.push_back(axisAlignedBoundingBoxFromPositions(meshData.positions));
    }
    return AABBList;
}

std::vector<MeshBinary> meshesToBinary(const std::vector<MeshData>& meshes, const std::vector<AxisAlignedBoundingBox>& AABBList) {
    assert(meshes.size() == AABBList.size());
    std::vector<MeshBinary> meshesBinary;
    meshesBinary.reserve(meshes.size());

    for (size_t meshIndex = 0; meshIndex < meshes.size(); meshIndex++) {
        const MeshData& meshData = meshes[meshIndex];
        MeshBinary meshBinary;
        meshBinary.texturePaths = meshData.texturePaths;
        meshBinary.boundingBox = AABBList[meshIndex];
		meshBinary.meanAlbedo = meshData.meanAlbedo;

        //index buffer
        meshBinary.indexCount = (uint32_t)meshData.indices.size();
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
            meshBinary.indexBuffer.resize((size_t)meshBinary.indexCount * (size_t)entryPerIndex);
            const size_t copySize = sizeof(uint32_t) * meshBinary.indexCount;
            memcpy(meshBinary.indexBuffer.data(), meshData.indices.data(), copySize);
        }

        //vertex buffer
        assert(meshData.positions.size() == meshData.uvs.size());
        assert(meshData.positions.size() == meshData.normals.size());
        assert(meshData.positions.size() == meshData.tangents.size());
        assert(meshData.positions.size() == meshData.bitangents.size());

        meshBinary.vertexCount = (uint32_t)meshData.positions.size();

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
            {
                const NormalizedR10G10B10A2 normalCompressed = vec3ToNormalizedR10B10G10A2(meshData.normals[i]);

                meshBinary.vertexBuffer.push_back(((uint8_t*)&normalCompressed)[0]);
                meshBinary.vertexBuffer.push_back(((uint8_t*)&normalCompressed)[1]);
                meshBinary.vertexBuffer.push_back(((uint8_t*)&normalCompressed)[2]);
                meshBinary.vertexBuffer.push_back(((uint8_t*)&normalCompressed)[3]);
            }

            //tangent stored as 32 bit R10G10B10A2
            {
                const NormalizedR10G10B10A2 tangentCompressed = vec3ToNormalizedR10B10G10A2(meshData.tangents[i]);

                meshBinary.vertexBuffer.push_back(((uint8_t*)&tangentCompressed)[0]);
                meshBinary.vertexBuffer.push_back(((uint8_t*)&tangentCompressed)[1]);
                meshBinary.vertexBuffer.push_back(((uint8_t*)&tangentCompressed)[2]);
                meshBinary.vertexBuffer.push_back(((uint8_t*)&tangentCompressed)[3]);
            }

            //bitangent stored as 32 bit R10G10B10A2
            {
                const NormalizedR10G10B10A2 bitangentCompressed = vec3ToNormalizedR10B10G10A2(meshData.bitangents[i]);

                meshBinary.vertexBuffer.push_back(((uint8_t*)&bitangentCompressed)[0]);
                meshBinary.vertexBuffer.push_back(((uint8_t*)&bitangentCompressed)[1]);
                meshBinary.vertexBuffer.push_back(((uint8_t*)&bitangentCompressed)[2]);
                meshBinary.vertexBuffer.push_back(((uint8_t*)&bitangentCompressed)[3]);
            }
        }
        meshesBinary.push_back(meshBinary);
    }

    return meshesBinary;
}