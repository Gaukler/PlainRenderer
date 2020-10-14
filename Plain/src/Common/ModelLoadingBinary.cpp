#include "pch.h"
#include "ModelLoadingBinary.h"

#include "Utilities/DirectoryUtils.h"
#include "Runtime/Rendering/Backend/VertexInput.h"

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

    const auto fullPath = DirectoryUtils::getResourceDirectory() / filename;
    std::ofstream file(fullPath, std::ios::binary);
    file.write((char*)fileData, fileSize);
    file.close();
}

bool loadBinaryMeshData(const std::filesystem::path& filename, std::vector<MeshBinary>* outMeshes) {
    const auto fullPath = DirectoryUtils::getResourceDirectory() / filename;
    std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "Could not open file: " << fullPath << "\n";
        return false;
    }

    const size_t fileSize = file.tellg();
    file.seekg(0, file.beg);

    ModelFileHeader header;
    file.read((char*)&header, sizeof(header));

    if (header.magicNumber != binaryModelMagicNumber) {
        std::cout << "Binary model file validation failed: " << fullPath << "\n";
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