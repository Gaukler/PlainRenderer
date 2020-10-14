#include "pch.h"
#include "ModelImport.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "Utilities/DirectoryUtils.h"
#include "Common/MeshProcessing.h"

bool loadModelOBJ(const std::filesystem::path& filename, std::vector<MeshData>* outData) {

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
            (*outData)[i].texturePaths.albedoTexturePath = modelDirectory / diffuseTexture;
        }
        else {
            (*outData)[i].texturePaths.albedoTexturePath = "";
        }

        if (material.bump_texname.length() != 0) {
            std::string normalTexture = cleanPath(material.bump_texname);
            (*outData)[i].texturePaths.normalTexturePath = modelDirectory / normalTexture;
        }
        else {
            (*outData)[i].texturePaths.normalTexturePath = "";
        }

        if (material.specular_texname.length() != 0) {
            std::string specularTexture = cleanPath(material.specular_texname);
            (*outData)[i].texturePaths.specularTexturePath = modelDirectory / specularTexture;
        }
        else {
            (*outData)[i].texturePaths.specularTexturePath = "";
        }
    }

    return true;
}