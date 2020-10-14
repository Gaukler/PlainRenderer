#include "pch.h"
#include "ModelImport.h"
#include "Common/ModelLoadSaveBinary.h"
#include "Common/MeshProcessing.h"
#include "Utilities/DirectoryUtils.h"

int main() {

    DirectoryUtils::init();
    const std::string inputFile = "Models\\Sponza\\Sponza.obj";
    const std::string outputFile = "Models\\Sponza\\Sponza.bin";

    std::vector<MeshData> meshData;
    if (loadModelOBJ(inputFile, &meshData)) {
        std::vector<MeshBinary> meshesBinary = meshesToBinary(meshData);
        saveBinaryMeshData(outputFile, meshesBinary);
        std::cout << "Input model: " << inputFile << "\n";
        std::cout << "Sucessfully converted model to binary format\n";
        std::cout << "Saved binary file: " << outputFile << "\n";
    }
    return 0;
}