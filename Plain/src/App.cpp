#include "pch.h"
#include "App.h"

#include "Utilities/DirectoryUtils.h"
#include "Utilities/Timer.h"
#include "CameraController.h"
#include "ModelLoader.h"
#include "Utilities/MathUtils.h"

/*
=========
App
=========
*/
App::App() {

    DirectoryUtils::init();

    const uint32_t initialWidth = 1600;
    const uint32_t initialHeight = 900;

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	m_window = glfwCreateWindow(initialWidth, initialHeight, "Plain Renderer", nullptr, nullptr);
    m_renderer.setup(m_window);

    {
        //std::filesystem::path modelPath = "Models\\cerberus\\cerberus.obj";
        std::filesystem::path modelPath = "Models\\Sponza\\Sponza.obj";
        //std::filesystem::path modelPath = "Models\\Bistro\\exterior.obj";

        std::vector<FrontendMeshHandle> mesh;
        std::vector<MeshData> meshData;
        if (loadModel(modelPath, &meshData)) {
            mesh = m_renderer.createMeshes(meshData);
        }

        m_meshes.insert(m_meshes.end(), mesh.begin(), mesh.end());
    }
}

/*
=========
run
=========
*/
void App::run() {

	CameraController cameraController(m_window);	

	while (!glfwWindowShouldClose(m_window)) {

        Timer::markNewFrame();

        m_renderer.newFrame();

		cameraController.update(m_window);

        //comment in to slowly rotate first mesh
        //if (m_meshes.size() >= 1) {
        //    m_renderer.setModelMatrix(m_meshes[0], glm::mat4(glm::rotate(glm::mat4(1.f), glm::radians(time) * 10.f, glm::vec3(0, -1, 0))));
        //}

        m_renderer.setCameraExtrinsic(cameraController.getExtrinsic());
        m_renderer.issueMeshDraws(m_meshes);
		m_renderer.renderFrame();

        glfwPollEvents();
	}
}

/*
=========
shutdown
=========
*/
void App::shutdown() {
	m_renderer.shutdown();
}