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

    const bool write = false;
    if (true) {
        std::vector<MeshData> meshData;
        if (loadModelOBJ("Models\\Sponza\\Sponza.obj", &meshData)) {
            std::vector<MeshBinary> meshesBinary = meshesToBinary(meshData);
            saveBinaryMeshData("Models\\Sponza\\Sponza.bin", meshesBinary);
            std::vector<glm::mat4> transforms(meshesBinary.size(), glm::mat4(1.f));
            gRenderFrontend.addStaticMeshes(meshesBinary, transforms);
        }
    }
    else {
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