#pragma once
#include "pch.h"
#include "Rendering/Camera.h"

#include "GLFW/glfw3.h"

/*
reads input and translates it into camera extrinsic(position + basis)
input is directly read from GLFW
not worth it to write an input handler for camera and a few hotkeys
*/
class CameraController {
public:
    void update();
    CameraExtrinsic getExtrinsic();

    //controls
    float m_movementSpeed = 1.f;
    float m_mouseSensitivity = 20.f;
    float m_controllerSensitivity = 100.f;
    float m_sprintSpeedFactor = 10.f;

private:
    float m_pitch = 0.f;
    float m_yaw = -90.f;

    CameraExtrinsic m_extrinsic;
};