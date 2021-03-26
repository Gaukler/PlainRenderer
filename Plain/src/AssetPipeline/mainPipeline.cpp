#include "pch.h"
#include "ModelImport.h"
#include "Common/ModelLoadSaveBinary.h"
#include "Common/MeshProcessing.h"
#include "Utilities/DirectoryUtils.h"
#include "SceneSDF.h"
#include "ImageIO.h"
#include "sdfUtilities.h"
#include "JobSystem.h"

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
	JobSystem::initJobSystem();

    std::filesystem::path binaryPathRelative =  settings.modelFilePath;
    binaryPathRelative.replace_extension("plain");

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

        std::cout << "Computing signed distance fields...\n";
        const SceneSDFTextures sceneSDFTextures = computeSceneSDFTextures(scene.meshes, AABBList);

		assert(sceneSDFTextures.descriptions.size() == scene.meshes.size());
		assert(sceneSDFTextures.descriptions.size() == sceneSDFTextures.data.size());

		for (size_t i = 0; i < sceneSDFTextures.descriptions.size(); i++) {
			const std::filesystem::path sdfTexturePath = scene.meshes[i].texturePaths.sdfTexturePath;
			if (sdfTexturePath.empty()) {
				continue;
			}
			//create directory if it doesn't exist
			const fs::path sdfTextureDirectory = sdfTexturePath.parent_path();
			if (!fs::exists(sdfTextureDirectory)) {
				fs::create_directories(sdfTextureDirectory);
			}
			writeDDSFile(sdfTexturePath, sceneSDFTextures.descriptions[i], sceneSDFTextures.data[i]);
			std::cout << "Saved SDF texture: "<< sdfTexturePath << "\n";
		}
    }
    return 0;
}