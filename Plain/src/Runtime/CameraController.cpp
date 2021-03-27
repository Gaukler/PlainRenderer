#include "pch.h"
#include "CameraController.h"
#include "Timer.h"
#include "InputManager.h"

CameraExtrinsic CameraController::getExtrinsic() {
	return m_extrinsic;
}

void CameraController::update() {
	//look
	if (gInputManager.isMouseButtonDown(MouseButton::right)) {
        const glm::vec2 mouseMovement = gInputManager.getMouseMovement();
		m_yaw   += m_mouseSensitivity * mouseMovement.x;
		m_pitch -= m_mouseSensitivity * mouseMovement.y;
	}
    //compute rotation
    {
        //limit look up/down
        const float pitchMin = -85.f;
        const float pitchMax = 85.f;
        m_pitch = glm::min(glm::max(m_pitch, pitchMin), pitchMax);

        const float yawRadian = glm::radians(m_yaw);
        const float pitchRadian = glm::radians(m_pitch);

        //construct coordinate system
        m_extrinsic.forward = glm::vec3(
            cos(pitchRadian) * cos(yawRadian),
            -sin(pitchRadian),
            cos(pitchRadian) * sin(yawRadian));

        m_extrinsic.up = glm::vec3(0.f, -1.f, 0.f);
        m_extrinsic.right = glm::normalize(glm::cross(m_extrinsic.up, m_extrinsic.forward));
        m_extrinsic.up = glm::cross(m_extrinsic.forward, m_extrinsic.right);
    }
    //movement
    float speedFactor = 1.f;
    if (gInputManager.isKeyboardKeyDown(KeyboardKey::keyShiftLeft)) {
        speedFactor = m_sprintSpeedFactor;
    }

    float deltaTime = Timer::getDeltaTimeFloat();
    //camera looks toward negative z-axis so to go into view direction -> position -= forward
    if (gInputManager.isKeyboardKeyDown(KeyboardKey::keyW)) {
        m_extrinsic.position += m_extrinsic.forward * m_movementSpeed * deltaTime * speedFactor;
    }
    if (gInputManager.isKeyboardKeyDown(KeyboardKey::keyS)) {
        m_extrinsic.position -= m_extrinsic.forward * m_movementSpeed * deltaTime * speedFactor;
    }
    if (gInputManager.isKeyboardKeyDown(KeyboardKey::keyD)) {
        m_extrinsic.position += m_extrinsic.right * m_movementSpeed * deltaTime * speedFactor;
    }
    if (gInputManager.isKeyboardKeyDown(KeyboardKey::keyA)) {
        m_extrinsic.position -= m_extrinsic.right * m_movementSpeed * deltaTime * speedFactor;
    }
}