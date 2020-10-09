#pragma once
#include "pch.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vulkan/vulkan.h>

#include "Rendering/RenderFrontend.h"
#include "CameraController.h"

class App {
public:
	App();
    void setup(GLFWwindow* window);
	void runUpdate();
private:
    CameraController m_cameraController;
    std::vector<FrontendMeshHandle> m_meshes;
};