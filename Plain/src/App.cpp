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
        const auto mesh = m_renderer.createMeshes(loadModel("Models\\Sponza\\Sponza.obj"));
        //const auto mesh = m_renderer.createMeshes(loadModel("Models\\DamagedHelmet\\DamagedHelmet.obj"));
        //const auto mesh = m_renderer.createMeshes(loadModel("Models\\Bistro\\exterior.obj"));

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
        m_renderer.issueMeshDraws(m_meshes, m_modelMatrices);
		m_renderer.renderFrame();

        glfwPollEvents();
	}
}

void App::cleanup() {
	m_renderer.teardown();
}