#pragma once
#include "pch.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vulkan/vulkan.h>

#include "Rendering/RenderFrontend.h"

class App {
public:
	App();
	void run();
	void cleanup();
private:
	GLFWwindow* m_window;
	RenderFrontend m_renderer;
    std::vector<MeshHandle> m_meshes;
};