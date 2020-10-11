#include "pch.h"
#include "App.h"

#include "Utilities/DirectoryUtils.h"
#include "Utilities/Timer.h"
#include "ModelLoader.h"
#include "Utilities/MathUtils.h"

App::App() {
    //load static scene
    std::filesystem::path paths[] = {
        //"Models\\cerberus\\cerberus.obj",
        "Models\\Sponza\\Sponza.obj",
        //"Models\\Bistro\\exterior.obj"
    };

    for(const auto file : paths)
    {
        std::vector<MeshData> meshData;
        if (loadModel(file, &meshData)) {
            std::vector<glm::mat4> transforms(meshData.size(), glm::mat4(1.f));
            gRenderFrontend.addStaticMeshes(meshData, transforms);
        }
    }
}

void App::runUpdate() {
    m_cameraController.update();
    gRenderFrontend.setCameraExtrinsic(m_cameraController.getExtrinsic());    
    gRenderFrontend.renderStaticMeshes();
}