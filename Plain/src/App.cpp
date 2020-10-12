#include "pch.h"
#include "App.h"

#include "Utilities/DirectoryUtils.h"
#include "Utilities/Timer.h"
#include "ModelLoader.h"
#include "Utilities/MathUtils.h"
#include "AssetPipeline/MeshProcessing.h"

App::App() {
    
}

void App::setup() {
    //load static scene
    std::filesystem::path paths[] = {
        //"Models\\cerberus\\cerberus.obj",
        "Models\\Sponza\\Sponza.obj",
        //"Models\\Bistro\\exterior.obj"
    };

    for (const auto file : paths)
    {
        std::vector<MeshData> meshData;
        if (loadModelOBJ(file, &meshData)) {
            std::vector<MeshBinary> meshesBinary = meshesToBinary(meshData);
            std::vector<glm::mat4> transforms(meshData.size(), glm::mat4(1.f));
            gRenderFrontend.addStaticMeshes(meshesBinary, transforms);
        }
    }
    gRenderFrontend.bakeSkyOcclusion();
}

void App::runUpdate() {
    m_cameraController.update();
    gRenderFrontend.setCameraExtrinsic(m_cameraController.getExtrinsic());    
    gRenderFrontend.renderStaticMeshes();
}