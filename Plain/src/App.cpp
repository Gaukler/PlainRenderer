#include "pch.h"
#include "App.h"

#include "Utilities/DirectoryUtils.h"
#include "Utilities/Timer.h"
#include "ModelLoader.h"
#include "Utilities/MathUtils.h"

App::App() {

    std::filesystem::path paths[] = {
        //"Models\\cerberus\\cerberus.obj",
        "Models\\Sponza\\Sponza.obj",
        //"Models\\Bistro\\exterior.obj"
    };

    for(const auto file : paths)
    {
        std::vector<FrontendMeshHandle> mesh;
        std::vector<MeshData> meshData;
        if (loadModel(file, &meshData)) {
            mesh = gRenderFrontend.createMeshes(meshData);
        }

        m_meshes.insert(m_meshes.end(), mesh.begin(), mesh.end());
    }
}

void App::runUpdate() {

    m_cameraController.update();
    gRenderFrontend.setCameraExtrinsic(m_cameraController.getExtrinsic());

    //comment in to move first mesh
    //if (m_meshes.size() >= 1) {
    //    glm::mat4 m = glm::translate(glm::mat4(1.f), glm::vec3(cos(time), -1.f, 0.f));
    //    m = glm::scale(m, glm::vec3(2.f));
    //    //m = glm::mat4(glm::rotate(glm::mat4(1.f), glm::radians(time) * 10.f, glm::vec3(0, -1, 0))
    //    m_renderer.setModelMatrix(m_meshes[0], m);
    //}
    
    gRenderFrontend.issueMeshDraws(m_meshes);
}