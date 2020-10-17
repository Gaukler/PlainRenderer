#include "pch.h"
#include "App.h"
#include "Timer.h"
#include "Utilities/DirectoryUtils.h"
#include "InputManager.h"
#include "Common/TypeConversion.h"

//expected command line arguments:
//argv[0] = executablePath
//argv[1] = window width
//argv[2] = window height
//argv[3] = binary scene file path
struct CommandLineSettings {
    int width = 0;
    int height = 0;
    std::string sceneFilePath;
};

CommandLineSettings parseCommandLineArguments(const int argc, char* argv[]) {
    std::filesystem::path executablePath = argv[0];

    CommandLineSettings settings;

    const int defaultWidth = 1280;
    const int defaultHeight = 720;

    if (argc < 3) {
        std::cout << "Missing command line arguments, using default values for width, height and scene file path\n";
        settings.width = defaultWidth;
        settings.height = defaultHeight;
        return settings;
    }

    if (!charArrayToInt(argv[1], &settings.width)) {
        std::cout << "Failed to parse command line argument width, using default value\n";
        settings.width = defaultWidth;
    }

    if (!charArrayToInt(argv[2], &settings.height)) {
        std::cout << "Failed to parse command line argument height, using default value\n";
        settings.height = defaultHeight;
    }

    if (argc < 4) {
        std::cout << "Missing command line argument, leaving scene file path at default value\n";
        return settings;
    }
    settings.sceneFilePath = argv[3];
    
    return settings;
}

void main(int argc, char* argv[]){

    const CommandLineSettings settings = parseCommandLineArguments(argc, argv);

    DirectoryUtils::init();

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(settings.width, settings.height, "Plain Renderer", nullptr, nullptr);

    gRenderBackend.setup(window);
    gRenderFrontend.setup(window);
    gInputManager.setup(window);

	App app;
    app.setup(settings.sceneFilePath);

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