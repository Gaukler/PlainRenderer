#include "pch.h"
#include "ModelLoader.h"

#include "Utilities/DirectoryUtils.h"
#include "ImageLoader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

bool loadModel(const std::filesystem::path& filename, std::vector<MeshData>* outData){

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
        return false;
    }

    std::vector<uint32_t> materialIndices;

    //iterate over models
    for (uint32_t shapeIndex = 0; shapeIndex < shapes.size(); shapeIndex++) {

        std::vector<MeshData> meshPerMaterial;
        meshPerMaterial.resize(materials.size());
        const auto shape = shapes[shapeIndex];

        //iterate over face vertices
        for (size_t i = 0; i < shape.mesh.indices.size(); i++) {

            int materialIndex = shape.mesh.material_ids[i / 3];

            const auto& indices = shape.mesh.indices[i];
            size_t vertexIndex = indices.vertex_index;
            size_t normalIndex = indices.normal_index;
            size_t uvIndex = indices.texcoord_index;

            meshPerMaterial[materialIndex].positions.push_back(glm::vec3(
                attributes.vertices[vertexIndex * 3],
                attributes.vertices[vertexIndex * 3 + 1],
                -attributes.vertices[vertexIndex * 3 + 2]));

            meshPerMaterial[materialIndex].normals.push_back(glm::vec3(
                attributes.normals[normalIndex * 3],
                attributes.normals[normalIndex * 3 + 1],
                -attributes.normals[normalIndex * 3 + 2]));

            meshPerMaterial[materialIndex].uvs.push_back(glm::vec2(
                attributes.texcoords[uvIndex * 2],
                1.f - attributes.texcoords[uvIndex * 2 + 1]));

            //correct for vulkan coordinate system
            meshPerMaterial[materialIndex].positions.back().y *= -1.f;
            meshPerMaterial[materialIndex].normals.back().y *= -1.f;
        }

        for (int i = 0; i < meshPerMaterial.size(); i++) {
            const auto& mesh = meshPerMaterial[i];
            if (mesh.positions.size() > 0) {
                outData->push_back(mesh);
                materialIndices.push_back(i);
            }
        }
    }

    const auto modelDirectory = fullPath.parent_path();
    for (size_t i = 0; i < outData->size(); i++) {
        computeTangentBitangent(&(*outData)[i]);
        (*outData)[i] = buildIndexedData((*outData)[i]);

        const auto cleanPath = [](std::string path) {

            auto backslashPos = path.find("\\\\");
            while (backslashPos != std::string::npos) {
                path.replace(backslashPos, 2, "\\");
                backslashPos = path.find("\\\\");
            }

            if (path.substr(0, 3) == "..\\") {
                return path.substr(3);
            }
            else {
                return path;
            }
        };

        const auto materialIndex = materialIndices[i];
        const auto material = materials[materialIndex];

        if (material.diffuse_texname.length() != 0) {
            std::filesystem::path diffuseTexture = cleanPath(material.diffuse_texname);
            (*outData)[i].material.albedoTexturePath = modelDirectory / diffuseTexture;
        }
        else {
            (*outData)[i].material.albedoTexturePath = "";
        }

        if (material.bump_texname.length() != 0) {
            std::string normalTexture = cleanPath(material.bump_texname);
            (*outData)[i].material.normalTexturePath = modelDirectory / normalTexture;
        }
        else {
            (*outData)[i].material.normalTexturePath = "";
        }

        if (material.specular_texname.length() != 0) {
            std::string specularTexture = cleanPath(material.specular_texname);
            (*outData)[i].material.specularTexturePath = modelDirectory / specularTexture;
        }
        else {
            (*outData)[i].material.specularTexturePath = "";
        }
    }
    
    return true;
}

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
        //iterate all indexed vertices
        for (uint32_t i = 0; i < data.positions.size(); i++) {

            glm::vec2 uvDiff = abs(data.uvs[i] - uv);
            if (uvDiff.x + uvDiff.y < 0.01f) {
                continue;
            }

            glm::vec3 posDiff = abs(data.positions[i] - positionVector);
            if (posDiff.x + posDiff.y + posDiff.z < 0.01f){
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
        indexedData.tangents[i]     = glm::normalize(indexedData.tangents[i]);
        indexedData.bitangents[i]   = glm::normalize(indexedData.bitangents[i]);
    }

    return indexedData;
}