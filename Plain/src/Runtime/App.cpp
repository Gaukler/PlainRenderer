#include "pch.h"
#include "App.h"

#include "Utilities/DirectoryUtils.h"
#include "Timer.h"
#include "Common/ModelLoadingBinary.h"
#include "Utilities/MathUtils.h"

App::App() {
    
}

void App::setup() {
    //load static scene
    {
        std::vector<MeshBinary> meshesBinary;
        loadBinaryMeshData("Models\\Sponza\\Sponza.bin", &meshesBinary);
        std::vector<glm::mat4> transforms(meshesBinary.size(), glm::mat4(1.f));
        gRenderFrontend.addStaticMeshes(meshesBinary, transforms);
    }
    
    gRenderFrontend.bakeSkyOcclusion();
}

void App::runUpdate() {
    m_cameraController.update();
    gRenderFrontend.setCameraExtrinsic(m_cameraController.getExtrinsic());    
    gRenderFrontend.renderStaticMeshes();
}