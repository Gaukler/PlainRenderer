#include "pch.h"
#include "App.h"
#include "Utilities/Timer.h"
#include "Utilities/DirectoryUtils.h"
#include "InputManager.h"

void main(){

    DirectoryUtils::init();

    const uint32_t initialWidth = 1600;
    const uint32_t initialHeight = 900;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(initialWidth, initialHeight, "Plain Renderer", nullptr, nullptr);

    gRenderBackend.setup(window);
    gRenderFrontend.setup(window);
    gInputManager.setup(window);

	App app;

    while (!glfwWindowShouldClose(window)) {
        Timer::markNewFrame();
        gRenderFrontend.prepareNewFrame();
        gInputManager.update();
        app.runUpdate();
        gRenderFrontend.renderFrame();
        glfwPollEvents();
    }

    gRenderFrontend.shutdown();
    gRenderBackend.shutdown();
}