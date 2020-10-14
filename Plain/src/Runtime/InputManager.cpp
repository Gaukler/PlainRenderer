#include "pch.h"
#include "InputManager.h"
#include "Timer.h"

//definition of extern variable from header
InputManager gInputManager;


InputManager::InputManager() {
    m_window = nullptr;
}

void InputManager::setup(GLFWwindow* window) {
    m_window = window;
}

void InputManager::shutdown() {

}

void InputManager::update() {
    //mouse position and movement
    {
        const glm::vec2 lastFrameMousePosition = m_mousePosition;

        double mousePosX;
        double mousePosY;
        glfwGetCursorPos(m_window, &mousePosX, &mousePosY);
        m_mousePosition = glm::vec2(mousePosX, mousePosY);
        m_mouseMovement = m_mousePosition - lastFrameMousePosition;

        const float deltaTime = Timer::getDeltaTimeFloat();
        m_mouseMovement *= deltaTime;
    }
    //mouse buttons
    for (int i = 0; i < MOUSE_BUTTON_COUNT; i++) {
        m_mouseButtonStatus[i] = glfwGetMouseButton(m_window, mouseButtonCodeToGLFW[i]) == GLFW_PRESS;
    }
    //keyboard buttons
    for (int i = 0; i < KEYBOARD_KEY_COUNT; i++) {
        m_keyboardStatus[i] = glfwGetKey(m_window, keyCodeToGLFW[i]) == GLFW_PRESS;
    }
}

glm::vec2 InputManager::getMouseMovement() {
    return m_mouseMovement;
}

glm::vec2 InputManager::getMousePosition() {
    return m_mousePosition;
}

bool InputManager::getMouseButton(const MouseButton button) {
    assert((size_t)button < MOUSE_BUTTON_COUNT);
    return m_mouseButtonStatus[(size_t)button];
}

bool InputManager::getKeyboardKey(const KeyboardKey key) {
    assert((size_t)key < KEYBOARD_KEY_COUNT);
    return m_keyboardStatus[(size_t)key];
}