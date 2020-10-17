#include "pch.h"
#include "ModelImport.h"
#include "Common/ModelLoadSaveBinary.h"
#include "Common/MeshProcessing.h"
#include "Utilities/DirectoryUtils.h"

//expected command line arguments:
//argv[0] = executablePath
//argv[1] = .obj scene file path
struct CommandLineSettings {
    std::string modelFilePath;
};

CommandLineSettings parseCommandLineArguments(const int argc, char* argv[]) {
    std::filesystem::path executablePath = argv[0];
    
    CommandLineSettings settings;
    if (argc < 2) {
        std::cout << "Missing command line parameter, scene file path not set\n";
    }
    settings.modelFilePath = argv[1];
    return settings;
}

int main(const int argc, char* argv[]) {

    CommandLineSettings settings = parseCommandLineArguments(argc, argv);

    DirectoryUtils::init();
    
    std::filesystem::path outputFile =  settings.modelFilePath;
    outputFile.replace_extension("bin");

    std::vector<MeshData> meshData;
    if (loadModelOBJ(settings.modelFilePath, &meshData)) {
        std::vector<MeshBinary> meshesBinary = meshesToBinary(meshData);
        saveBinaryMeshData(outputFile, meshesBinary);
        std::cout << "Input model: " << settings.modelFilePath << "\n";
        std::cout << "Sucessfully converted model to binary format\n";
        std::cout << "Saved binary file: " << outputFile << "\n";
    }
    return 0;
}