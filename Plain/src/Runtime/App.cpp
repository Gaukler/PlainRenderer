#include "pch.h"
#include "App.h"

#include "Utilities/DirectoryUtils.h"
#include "Timer.h"
#include "Common/ModelLoadSaveBinary.h"
#include "Utilities/MathUtils.h"
#include "Common/sdfUtilities.h"
#include "ImageIO.h"

App::App() {
    
}

void App::setup(const std::string& sceneFilePath) {
    //load static scene
    {
        std::vector<MeshBinary> meshesBinary;
        std::cout << "Loading scene file: " << sceneFilePath << "\n";
        loadBinaryMeshData(sceneFilePath, &meshesBinary);
        std::vector<glm::mat4> transforms(meshesBinary.size(), glm::mat4(1.f));
        gRenderFrontend.addStaticMeshes(meshesBinary, transforms);

        const std::filesystem::path sceneSDFPathRelative = binaryToSDFPath(sceneFilePath);
        std::cout << "Loading scene sdf: " << sceneSDFPathRelative << "\n";

        ImageDescription sceneSDF;
        if (loadDDSFile(DirectoryUtils::getResourceDirectory() / sceneSDFPathRelative, &sceneSDF)) {
            gRenderFrontend.setSceneSDF(sceneSDF);
        }
        else {
            std::cout << "Failed to load scene sdf\n";
        }
    }
    
    gRenderFrontend.bakeSkyOcclusion();
	gRenderFrontend.bakeSceneMaterialVoxelTexture();
}

void App::runUpdate() {
    m_cameraController.update();
    gRenderFrontend.setCameraExtrinsic(m_cameraController.getExtrinsic());    
    gRenderFrontend.prepareForDrawcalls();
    gRenderFrontend.renderStaticMeshes();
}