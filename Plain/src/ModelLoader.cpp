#include "pch.h"
#include "ModelLoader.h"

#include "Utilities/DirectoryUtils.h"
#include "ImageLoader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

/*
=========
loadModel
=========
*/
MeshData loadModel(const std::filesystem::path& filename){

    const std::filesystem::path fullPath = DirectoryUtils::getResourceDirectory() / filename;

    tinyobj::attrib_t attributes;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warning;
    std::string error;

    //parent path removes the filename, so the .mtl file is searched in the directory of the .obj file
    bool success = tinyobj::LoadObj(&attributes, &shapes, &materials, &warning, &error, fullPath.string().c_str(), fullPath.parent_path().string().c_str());

    if (!warning.empty()) {
        std::cout << warning << std::endl;
    }

    if (!error.empty()) {
        std::cerr << error << std::endl;
    }

    if (!success) {
        throw std::runtime_error("couldn't open file");
    }

    MeshData meshData;

    //iterate over models
    for (const auto& shape : shapes) {
        //iterate over face vertices
        for (size_t i = 0; i < shape.mesh.indices.size(); i++) {

            const auto& indices = shape.mesh.indices[i];
            size_t vertexIndex = indices.vertex_index;
            size_t normalIndex = indices.normal_index;
            size_t uvIndex = indices.texcoord_index;

            meshData.positions.push_back(glm::vec3(
                attributes.vertices[vertexIndex * 3],
                attributes.vertices[vertexIndex * 3 + 1],
                attributes.vertices[vertexIndex * 3 + 2]));

            meshData.normals.push_back(glm::vec3(
                attributes.normals[normalIndex * 3],
                attributes.normals[normalIndex * 3 + 1],
                attributes.normals[normalIndex * 3 + 2]));

            meshData.uvs.push_back(glm::vec2(
                attributes.texcoords[uvIndex * 2],
                attributes.texcoords[uvIndex * 2 + 1]));

            //correct for vulkan coordinate system
            meshData.positions.back().y *= -1.f;
            meshData.normals.back().y *= -1.f;
        }
    }
    computeTangentBitangent(&meshData);
    MeshData indexedData = buildIndexedData(meshData);

    

    /*
    textures
    note that ***_tex_name is based on blender exported .objs which has incorrect naming for certain texture types
    */
    const auto modelDirectory = fullPath.parent_path();

    std::string diffuseTexture = materials[0].diffuse_texname;
    indexedData.material.diffuseTexture = loadImage(modelDirectory / diffuseTexture, true);

    std::string normalTexture = materials[0].bump_texname;
    indexedData.material.normalTexture = loadImage(modelDirectory / normalTexture, true);

    std::string metalicTexture = materials[0].reflection_texname;
    indexedData.material.metalicTexture = loadImage(modelDirectory / metalicTexture, true);

    std::string roughnessTexture = materials[0].specular_highlight_texname;
    indexedData.material.roughnessTexture = loadImage(modelDirectory / roughnessTexture, true);

    return indexedData;
}

/*
=========
computeTangentBitangent
=========
*/
void computeTangentBitangent(MeshData* outMeshData) {
    assert(outMeshData != nullptr);
    assert(outMeshData->positions.size() == outMeshData->uvs.size());
    assert(outMeshData->tangents.size() == 0);
    assert(outMeshData->bitangents.size() == 0);

    for (uint32_t triangle = 0; triangle < outMeshData->positions.size() / 3; triangle++) {

        const uint32_t i0 = triangle * 3;
        const uint32_t i1 = triangle * 3 + 1;
        const uint32_t i2 = triangle * 3 + 2;
        
        const glm::vec3 v0 = outMeshData->positions[i0];
        const glm::vec3 v1 = outMeshData->positions[i1];
        const glm::vec3 v2 = outMeshData->positions[i2];

        const glm::vec2 uv0 = outMeshData->uvs[i0];
        const glm::vec2 uv1 = outMeshData->uvs[i1];
        const glm::vec2 uv2 = outMeshData->uvs[i2];
        
        const glm::vec3 edge1 = v1 - v0;
        const glm::vec3 edge2 = v2 - v0;
        const glm::vec2 uvDelta1 = uv1 - uv0;
        const glm::vec2 uvDelta2 = uv2 - uv0;

        const float denom = 1.f / (uvDelta1.x * uvDelta2.y - uvDelta1.y * uvDelta2.x);
        const glm::vec3 tangent     = denom * (uvDelta2.y * edge1 - uvDelta1.y * edge2);
        const glm::vec3 bitangent   = denom * (uvDelta1.x * edge2 - uvDelta2.x * edge1);

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

    auto findSimilar = [](const glm::vec3& positionVector, const glm::vec2& uv, const glm::vec3& normal, const MeshData &data, uint32_t* outSimilarIndex) {
        /*
        iterate all indexed vertices
        */
        for (uint32_t i = 0; i < data.positions.size(); i++) {

            glm::vec3 posDiff = abs(data.positions[i] - positionVector);
            glm::vec2 uvDiff = abs(data.uvs[i] - uv);
            glm::vec3 normalDiff = abs(data.normals[i] - normal);

            bool similarPosition = (posDiff.x + posDiff.y + posDiff.z < 0.01f);
            bool similarUvs = (uvDiff.x + uvDiff.y < 0.01f);
            bool similarNormals = (normalDiff.x + normalDiff.y + normalDiff.z < 0.01f);

            if (similarPosition && similarUvs && similarNormals) {
                *outSimilarIndex = i;
                return true;
            }
        }
        return false;
    };

    /*
    iterate over all vertices
    */
    MeshData indexedData;
    uint32_t newIndex = 0;
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
    for (uint32_t i = 0; i < indexedData.tangents.size(); i++) {
        indexedData.tangents[i] = glm::normalize(indexedData.tangents[i]);
        indexedData.bitangents[i] = glm::normalize(indexedData.bitangents[i]);
    }

    return indexedData;
}