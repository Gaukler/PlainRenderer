#include "pch.h"
#include "ModelLoader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "Utilities/DirectoryUtils.h"
#include "ImageLoader.h"
#include "AssetPipeline/MeshProcessing.h"
#include "Rendering/Backend/VertexInput.h"

bool loadModelOBJ(const std::filesystem::path& filename, std::vector<MeshData>* outData){

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

const uint32_t binaryModelMagicNumber = *(uint32_t*)"PlMB"; //stands for Plain Model Binary

struct ModelFileHeader {
    uint32_t magicNumber;   //for verification
    size_t meshCount;
};

//Binary model file structure:
//ModelFileHeader
//header.meshCount times the following data structure:
//uint32_t indexCount
//uint32_t vertexCount
//uint32_t albedo texture path length
//char* albedo texture path
//uint32_t normal texture path length
//char* normal texture path
//uint32_t specular texture path length
//char* specular texture path
//index buffer data, as 16 bit or 32 bit unsigned int, uses 16 bit if index count < uint16_t::max
//vertex buffer data, vertexCount times full vertex format size

//copies data and returns offset + copy size
size_t copyToBuffer(const void* src, uint8_t* dst, const size_t copySize, const size_t offset) {
    memcpy(dst + offset, src, copySize);
    return offset + copySize;
}

void saveBinaryMeshData(const std::filesystem::path& filename, const std::vector<MeshBinary>& meshes) {
    ModelFileHeader header;
    header.magicNumber = binaryModelMagicNumber;
    header.meshCount = meshes.size();

    size_t dataSize = 0;
    for (const MeshBinary& meshBinary : meshes) {
        dataSize += sizeof(meshBinary.indexCount);
        dataSize += sizeof(meshBinary.vertexCount);
        dataSize += sizeof(meshBinary.boundingBox);
        dataSize += sizeof(uint32_t); //albedo texture path length
        dataSize += meshBinary.texturePaths.albedoTexturePath.string().size();
        dataSize += sizeof(uint32_t); //normal texture path length
        dataSize += meshBinary.texturePaths.normalTexturePath.string().size();
        dataSize += sizeof(uint32_t); //specular texture path length
        dataSize += meshBinary.texturePaths.specularTexturePath.string().size();
        dataSize += sizeof(uint16_t) * meshBinary.indexBuffer.size();
        dataSize += sizeof(uint8_t) * meshBinary.vertexBuffer.size();
    }

    const size_t fileSize = dataSize + sizeof(ModelFileHeader);
    uint8_t* fileData = new uint8_t[fileSize];

    size_t writePointer = 0;
    writePointer = copyToBuffer(&header, fileData, sizeof(header), writePointer);

    for(const MeshBinary& meshBinary : meshes){
        writePointer = copyToBuffer(&meshBinary.indexCount, fileData, sizeof(meshBinary.indexCount), writePointer);
        writePointer = copyToBuffer(&meshBinary.vertexCount, fileData, sizeof(meshBinary.vertexCount), writePointer);
        writePointer = copyToBuffer(&meshBinary.boundingBox, fileData, sizeof(meshBinary.boundingBox), writePointer);

        const uint32_t albedoPathLength = meshBinary.texturePaths.albedoTexturePath.string().size();
        writePointer = copyToBuffer(&albedoPathLength, fileData, sizeof(albedoPathLength), writePointer);

        writePointer = copyToBuffer(
            meshBinary.texturePaths.albedoTexturePath.string().c_str(),
            fileData, 
            meshBinary.texturePaths.albedoTexturePath.string().size() * sizeof(char), 
            writePointer);

        const uint32_t normalPathLength = meshBinary.texturePaths.normalTexturePath.string().size();
        writePointer = copyToBuffer(&normalPathLength, fileData, sizeof(normalPathLength), writePointer);

        writePointer = copyToBuffer(
            meshBinary.texturePaths.normalTexturePath.string().c_str(),
            fileData,
            meshBinary.texturePaths.normalTexturePath.string().size() * sizeof(char),
            writePointer);

        const uint32_t specularPathLength = meshBinary.texturePaths.specularTexturePath.string().size();
        writePointer = copyToBuffer(&specularPathLength, fileData, sizeof(specularPathLength), writePointer);

        writePointer = copyToBuffer(
            meshBinary.texturePaths.specularTexturePath.string().c_str(),
            fileData,
            meshBinary.texturePaths.specularTexturePath.string().size() * sizeof(char),
            writePointer);

        writePointer = copyToBuffer(
            meshBinary.indexBuffer.data(),
            fileData,
            sizeof(uint16_t) * meshBinary.indexBuffer.size(),
            writePointer);

        writePointer = copyToBuffer(
            meshBinary.vertexBuffer.data(),
            fileData,
            sizeof(uint8_t) * meshBinary.vertexBuffer.size(),
            writePointer);
    }

    assert(writePointer == fileSize);

    std::ofstream file(filename, std::ios::binary);
    file.write((char*)fileData, fileSize);
    file.close();
}

bool loadBinaryMeshData(const std::filesystem::path& filename, std::vector<MeshBinary>* outMeshes) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "Could not open file: " << filename << "\n";
        return false;
    }

    const size_t fileSize = file.tellg();
    file.seekg(0, file.beg);

    ModelFileHeader header;
    file.read((char*)&header, sizeof(header));

    if (header.magicNumber != binaryModelMagicNumber) {
        std::cout << "Binary model file validation failed: " << filename << "\n";
        file.close();
        return false;
    }

    outMeshes->reserve(header.meshCount);

    for (size_t i = 0; i < header.meshCount; i++) {
        MeshBinary mesh;
        file.read((char*)&mesh.indexCount, sizeof(mesh.indexCount));
        file.read((char*)&mesh.vertexCount, sizeof(mesh.vertexCount));
        file.read((char*)&mesh.boundingBox, sizeof(mesh.boundingBox));
    
        uint32_t albedoPathLength;
        file.read((char*)&albedoPathLength, sizeof(albedoPathLength));
        std::string albedoPathString;
        albedoPathString.resize(albedoPathLength);
        file.read(albedoPathString.data(), albedoPathLength * sizeof(char));
        mesh.texturePaths.albedoTexturePath = albedoPathString;

        uint32_t normalPathLength;
        file.read((char*)&normalPathLength, sizeof(normalPathLength));
        std::string normalPathString;
        normalPathString.resize(normalPathLength);
        file.read(normalPathString.data(), normalPathLength * sizeof(char));
        mesh.texturePaths.normalTexturePath = normalPathString;

        uint32_t specularPathLength;
        file.read((char*)&specularPathLength, sizeof(specularPathLength));
        std::string specularPathString;
        specularPathString.resize(specularPathLength);
        file.read(specularPathString.data(), specularPathLength * sizeof(char));
        mesh.texturePaths.specularTexturePath = specularPathString;

        size_t halfPerIndex;
        size_t bytePerIndex;
        if (mesh.indexCount < std::numeric_limits<uint16_t>::max()) {
            //indices fit into 16 bit 
            halfPerIndex = 1;
            bytePerIndex = 2;
        }
        else {
            //indices require 32 bit
            halfPerIndex = 2;
            bytePerIndex = 4;
        }
        mesh.indexBuffer.resize(mesh.indexCount * halfPerIndex);
        file.read((char*)mesh.indexBuffer.data(), mesh.indexCount * bytePerIndex);

        size_t vertexBufferSize = vertexFormatFullByteSize * mesh.vertexCount;
        mesh.vertexBuffer.resize(vertexBufferSize);
        file.read((char*)mesh.vertexBuffer.data(), vertexBufferSize);

        outMeshes->push_back(mesh);
    }

    file.close();
    return true;
}