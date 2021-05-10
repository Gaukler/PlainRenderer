#include "pch.h"
#include "ModelLoadSaveBinary.h"

#include "Utilities/DirectoryUtils.h"
#include "VertexInput.h"

const uint32_t binaryModelMagicNumber = *(uint32_t*)"PlMB"; // stands for Plain Model Binary

struct ModelFileHeader {
    uint32_t magicNumber;   // for verification
    size_t objectCount;
    size_t meshCount;
};

/*
Binary model file structure:
ModelFileHeader

header.objectCount time the Object struct which contains a 4x4 model matrix and a mesh index

header.meshCount times the following data structure:
uint32_t indexCount
uint32_t vertexCount
uint32_t albedo texture path length
char* albedo texture path
uint32_t normal texture path length
char* normal texture path
uint32_t specular texture path length
char* specular texture path
index buffer data, as 16 bit or 32 bit unsigned int, uses 16 bit if index count < uint16_t::max
vertex buffer data, vertexCount times full vertex format size
*/

// copies data and returns offset + copy size
size_t copyToBuffer(const void* src, uint8_t* dst, const size_t copySize, const size_t offset) {
    memcpy(dst + offset, src, copySize);
    return offset + copySize;
}

void saveBinaryScene(const std::filesystem::path& filename, SceneBinary scene){
    ModelFileHeader header;
    header.magicNumber = binaryModelMagicNumber;
    header.objectCount = scene.objects.size();
    header.meshCount = scene.meshes.size();

    size_t meshDataSize = 0;

    for (const MeshBinary& meshBinary : scene.meshes) {
        meshDataSize += sizeof(meshBinary.indexCount);
        meshDataSize += sizeof(meshBinary.vertexCount);
        meshDataSize += sizeof(meshBinary.boundingBox);
        meshDataSize += sizeof(uint32_t); // albedo texture path length
        meshDataSize += meshBinary.texturePaths.albedoTexturePath.string().size();
        meshDataSize += sizeof(uint32_t); // normal texture path length
        meshDataSize += meshBinary.texturePaths.normalTexturePath.string().size();
        meshDataSize += sizeof(uint32_t); // specular texture path length
        meshDataSize += meshBinary.texturePaths.specularTexturePath.string().size();
        meshDataSize += sizeof(uint32_t); // sdf texture path length
        meshDataSize += sizeof(meshBinary.meanAlbedo);
        meshDataSize += meshBinary.texturePaths.sdfTexturePath.string().size();
        meshDataSize += sizeof(uint16_t) * meshBinary.indexBuffer.size();
        meshDataSize += sizeof(uint8_t) * meshBinary.vertexBuffer.size();
    }

    const size_t objectDataSize = sizeof(ObjectBinary) * scene.objects.size();
    const size_t fileSize = sizeof(ModelFileHeader) + objectDataSize + meshDataSize;
    uint8_t* fileData = new uint8_t[fileSize];

    size_t writePointer = 0;
    writePointer = copyToBuffer(&header, fileData, sizeof(header), writePointer);               //write header
    writePointer = copyToBuffer(scene.objects.data(), fileData, objectDataSize, writePointer);  //write object data

    //write mesh data
    for(const MeshBinary& meshBinary : scene.meshes){
        writePointer = copyToBuffer(&meshBinary.indexCount, fileData, sizeof(meshBinary.indexCount), writePointer);
        writePointer = copyToBuffer(&meshBinary.vertexCount, fileData, sizeof(meshBinary.vertexCount), writePointer);
        writePointer = copyToBuffer(&meshBinary.boundingBox, fileData, sizeof(meshBinary.boundingBox), writePointer);

        const uint32_t albedoPathLength = (uint32_t)meshBinary.texturePaths.albedoTexturePath.string().size();
        writePointer = copyToBuffer(&albedoPathLength, fileData, sizeof(albedoPathLength), writePointer);

        writePointer = copyToBuffer(
            meshBinary.texturePaths.albedoTexturePath.string().c_str(),
            fileData, 
            meshBinary.texturePaths.albedoTexturePath.string().size() * sizeof(char), 
            writePointer);

        const uint32_t normalPathLength = (uint32_t)meshBinary.texturePaths.normalTexturePath.string().size();
        writePointer = copyToBuffer(&normalPathLength, fileData, sizeof(normalPathLength), writePointer);

        writePointer = copyToBuffer(
            meshBinary.texturePaths.normalTexturePath.string().c_str(),
            fileData,
            meshBinary.texturePaths.normalTexturePath.string().size() * sizeof(char),
            writePointer);

        const uint32_t specularPathLength = (uint32_t)meshBinary.texturePaths.specularTexturePath.string().size();
        writePointer = copyToBuffer(&specularPathLength, fileData, sizeof(specularPathLength), writePointer);

        writePointer = copyToBuffer(
            meshBinary.texturePaths.specularTexturePath.string().c_str(),
            fileData,
            meshBinary.texturePaths.specularTexturePath.string().size() * sizeof(char),
            writePointer);

        const uint32_t sdfPathLength = (uint32_t)meshBinary.texturePaths.sdfTexturePath.string().size();
        writePointer = copyToBuffer(&sdfPathLength, fileData, sizeof(sdfPathLength), writePointer);

        writePointer = copyToBuffer(
            meshBinary.texturePaths.sdfTexturePath.string().c_str(),
            fileData,
            meshBinary.texturePaths.sdfTexturePath.string().size() * sizeof(char),
            writePointer);

        writePointer = copyToBuffer(
            &meshBinary.meanAlbedo,
            fileData,
            sizeof(meshBinary.meanAlbedo),
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

    const auto fullPath = DirectoryUtils::getResourceDirectory() / filename;
    std::ofstream file(fullPath, std::ios::binary);
    file.write((char*)fileData, fileSize);
    file.close();
}

bool loadBinaryScene(const std::filesystem::path& filename, SceneBinary* outScene) {
    const auto fullPath = DirectoryUtils::getResourceDirectory() / filename;
    std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "Could not open file: " << fullPath << "\n";
        return false;
    }

    const size_t fileSize = file.tellg();
    file.seekg(0, file.beg);

    // read header
    ModelFileHeader header;
    file.read((char*)&header, sizeof(header));

    if (header.magicNumber != binaryModelMagicNumber) {
        std::cout << "Binary model file validation failed: " << fullPath << "\n";
        file.close();
        return false;
    }

    // read object data
    outScene->objects.resize(header.objectCount);
    const size_t objectDataSize = header.objectCount * sizeof(ObjectBinary);
    file.read((char*)outScene->objects.data(), objectDataSize);

    // read mesh data
    outScene->meshes.reserve(header.meshCount);

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

        uint32_t sdfPathLength;
        file.read((char*)&sdfPathLength, sizeof(sdfPathLength));
        std::string sdfPathString;
        sdfPathString.resize(sdfPathLength);
        file.read(sdfPathString.data(), sdfPathLength * sizeof(char));
        mesh.texturePaths.sdfTexturePath = sdfPathString;

        file.read((char*)&mesh.meanAlbedo, sizeof(mesh.meanAlbedo));

        size_t halfPerIndex;
        size_t bytePerIndex;
        if (mesh.indexCount < std::numeric_limits<uint16_t>::max()) {
            // indices fit into 16 bit 
            halfPerIndex = 1;
            bytePerIndex = 2;
        }
        else {
            // indices require 32 bit
            halfPerIndex = 2;
            bytePerIndex = 4;
        }
        mesh.indexBuffer.resize(mesh.indexCount * halfPerIndex);
        file.read((char*)mesh.indexBuffer.data(), mesh.indexCount * bytePerIndex);

        size_t vertexBufferSize = (size_t)getFullVertexFormatByteSize() * (size_t)mesh.vertexCount;
        mesh.vertexBuffer.resize(vertexBufferSize);
        file.read((char*)mesh.vertexBuffer.data(), vertexBufferSize);

        outScene->meshes.push_back(mesh);
    }

    file.close();
    return true;
}