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
    m_meshes.push_back(m_renderer.createMesh(loadModel("Models\\plain.obj")));
    //m_meshes.push_back(m_renderer.createMesh(loadModel("Models\\monkey.obj")));
    //m_meshes.push_back(m_renderer.createMesh(loadModel("Models\\ball.obj")));
    m_meshes.push_back(m_renderer.createMesh(loadModel("Models\\MaterialSamples.obj")));
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
        for (uint32_t i = 0; i < m_meshes.size(); i++) {
            const glm::mat4 m = i == 1 ? glm::translate(glm::mat4(1.f), glm::vec3(glm::sin(0), -1.f, 0.f)) : glm::mat4(1.f);
            m_renderer.issueMeshDraw(m_meshes[i], m);
        }
		m_renderer.renderFrame();

        glfwPollEvents();
	}
}

void App::cleanup() {
	m_renderer.teardown();
}