#pragma once
#include "pch.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vulkan/vulkan.h>

//window management using glfw
//only creation and management of one window is supported
namespace Window {
    void initWindowSystem();
    GLFWwindow* createWindow(const int width, const int height);
    void toggleFullscreen();
}