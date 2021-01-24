#include "pch.h"
#include "ModelImport.h"
#include "Common/ModelLoadSaveBinary.h"
#include "Common/MeshProcessing.h"
#include "Utilities/DirectoryUtils.h"
#include "SceneSDF.h"
#include "ImageIO.h"
#include "sdfUtilities.h"

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

    std::filesystem::path binaryPathRelative =  settings.modelFilePath;
    binaryPathRelative.replace_extension("plain");

    const std::filesystem::path sdfPathRelative = binaryToSDFPath(binaryPathRelative);

    Scene scene;
    std::cout << "Input model: " << settings.modelFilePath << "\n";
    if (loadModelGLTF(settings.modelFilePath, &scene)) {
        std::vector<AxisAlignedBoundingBox> AABBList = AABBListFromMeshes(scene.meshes);
		SceneBinary sceneBinary;
		sceneBinary.objects = scene.objects;
		sceneBinary.meshes = meshesToBinary(scene.meshes, AABBList);
        std::cout << "Sucessfully converted model to binary format\n";
        saveBinaryScene(binaryPathRelative, sceneBinary);
        std::cout << "Saved binary file: " << binaryPathRelative << "\n";

        std::cout << "Computing signed distance field...\n";
        const ImageDescription sceneSDFTexture = ComputeSceneSDFTexture(scene.meshes, AABBList);

        writeDDSFile(DirectoryUtils::getResourceDirectory() / sdfPathRelative, sceneSDFTexture);
        std::cout << "Saved SDF texture: " << sdfPathRelative << "\n";

    }
    return 0;
}