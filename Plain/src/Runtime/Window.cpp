#include "pch.h"
#include "Window.h"
#include "Rendering/RenderFrontend.h"

namespace Window {

    GLFWwindow* windowPtr = nullptr;
    GLFWmonitor* primaryMonitorPtr = nullptr;
    const GLFWvidmode* primaryMonitorVideoMode;

    struct WindowState {
        bool isFullscreen = false;
        glm::ivec2 lastWindowedPosition;	//saved when going to fullscreen
        glm::ivec2 lastWindowedResolution;	//saved when going to fullscreen
    };

    WindowState windowState;

    void initWindowSystem() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        primaryMonitorPtr = glfwGetPrimaryMonitor();
        primaryMonitorVideoMode = glfwGetVideoMode(primaryMonitorPtr);
    }

    GLFWwindow* createWindow(const int width, const int height) {
        assert(windowPtr == nullptr);
        windowPtr = glfwCreateWindow(width, height, "Plain Renderer", nullptr, nullptr);
        return windowPtr;
    }

    void toggleFullscreen() {
        assert(windowPtr);
        assert(primaryMonitorPtr);
        if (windowState.isFullscreen) {
            //switch to windowed
            glfwSetWindowMonitor(
                windowPtr,
                nullptr,
                windowState.lastWindowedPosition.x,
                windowState.lastWindowedPosition.y,
                windowState.lastWindowedResolution.x,
                windowState.lastWindowedResolution.y,
                primaryMonitorVideoMode->refreshRate);

            windowState.isFullscreen = false;
        }
        else {
            //back up resolution and position
            glfwGetWindowPos(
                windowPtr,
                &windowState.lastWindowedPosition.x,
                &windowState.lastWindowedPosition.y);
            glfwGetWindowSize(
                windowPtr,
                &windowState.lastWindowedResolution.x,
                &windowState.lastWindowedResolution.y);
            //switch to fullscreen
            glfwSetWindowMonitor(
                windowPtr, 
                primaryMonitorPtr, 
                0, 
                0, 
                primaryMonitorVideoMode->width, 
                primaryMonitorVideoMode->height, 
                primaryMonitorVideoMode->refreshRate);

            windowState.isFullscreen = true;
        }
    }

    std::array<int, 2> getGlfwWindowResolution(GLFWwindow* pWindow) {
        std::array<int, 2> resolution;
        glfwGetWindowSize(pWindow, &resolution[0], &resolution[1]);
        return resolution;
    }
}