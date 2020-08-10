#include "pch.h"
#include "App.h"

#include "Utilities/DirectoryUtils.h"
#include "Utilities/Timer.h"
#include "CameraController.h"
#include "ModelLoader.h"
#include <imgui/imgui.h>

App::App() {

    DirectoryUtils::init();

    const uint32_t initialWidth = 1600;
    const uint32_t initialHeight = 900;

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	m_window = glfwCreateWindow(initialWidth, initialHeight, "Plain Renderer", nullptr, nullptr);
    m_renderer.setup(m_window);

    {
        std::filesystem::path modelPath = "Models\\cerberus\\cerberus.obj";
        //std::filesystem::path modelPath = "Models\\Sponza\\Sponza.obj";
        //std::filesystem::path modelPath = "Models\\Bistro\\exterior.obj";

        std::vector<MeshHandle> mesh;
        std::vector<MeshData> meshData;
        if (loadModel(modelPath, &meshData)) {
            mesh = m_renderer.createMeshes(meshData);
        }

        m_meshes.insert(m_meshes.end(), mesh.begin(), mesh.end());
    }
    m_modelMatrices = std::vector<glm::mat4>(m_meshes.size(), glm::scale(glm::mat4(1.f), glm::vec3(1.f)));
}

void App::run() {

	Timer& timer = Timer::getReference();
	CameraController cameraController(m_window);	

	while (!glfwWindowShouldClose(m_window)) {

		timer.markNewFrame();
		float deltaTime = timer.getDeltaTimeFloat();
        float time = timer.getTime();

        m_renderer.newFrame();

		cameraController.update(m_window);

        m_renderer.setCameraExtrinsic(cameraController.getExtrinsic());
        //if (m_modelMatrices.size() == 1) { //only rotate when loading single test model
        //    m_modelMatrices[0] = glm::rotate(glm::mat4(1.f), timer.getTimeFloat(), glm::vec3(0.f, -1.f, 0.f));
        //}
        m_renderer.issueMeshDraws(m_meshes, m_modelMatrices);
		m_renderer.renderFrame();

        glfwPollEvents();
	}
}

void App::cleanup() {
	m_renderer.teardown();
}