#include "pch.h"
#include "CameraController.h"


#include "Utilities/Timer.h"

/*
=========
CameraController
=========
*/
CameraController::CameraController(GLFWwindow* inputWindow) {
	double mouseX, mouseY;
	glfwGetCursorPos(inputWindow, &mouseX, &mouseY);
	m_lastMousePositon = glm::vec2(mouseX, mouseY);
}

/*
=========
getExtrinsic
=========
*/
CameraExtrinsic CameraController::getExtrinsic() {
	return m_extrinsic;
}

/*
=========
update
=========
*/
void CameraController::update(GLFWwindow* inputWindow) {

	float deltaTime = Timer::getDeltaTimeFloat();

	//compute mouse delta
	double mouseX, mouseY;
	glfwGetCursorPos(inputWindow, &mouseX, &mouseY);
	glm::vec2 mousePos = glm::vec2(mouseX, mouseY);
	glm::vec2 mouseDelta = mousePos - m_lastMousePositon;
	m_lastMousePositon = mousePos;

	//look
	if (glfwGetMouseButton(inputWindow, GLFW_MOUSE_BUTTON_RIGHT)) {
		m_yaw   -= deltaTime * m_mouseSensitivity * mouseDelta.x;
		m_pitch -= deltaTime * m_mouseSensitivity * mouseDelta.y;
	}

	//limit look up/down
	float pitchMin = -85.f;
	float pitchMax = 85.f;
	m_pitch = glm::min(glm::max(m_pitch, pitchMin), pitchMax);

	const float yawRadian   = glm::radians(m_yaw);
	const float pitchRadian = glm::radians(m_pitch);

	//construct coordinate system
    m_extrinsic.forward = glm::vec3(
		cos(pitchRadian) * cos(yawRadian),
		sin(pitchRadian),
		cos(pitchRadian) * sin(yawRadian));

	m_extrinsic.up = glm::vec3(0.f, -1.f, 0.f);
    m_extrinsic.right = glm::normalize(glm::cross(m_extrinsic.up, m_extrinsic.forward));
    m_extrinsic.up = glm::cross(m_extrinsic.forward, m_extrinsic.right);

	//position
	float speedFactor = 1.f;
	if (glfwGetKey(inputWindow, GLFW_KEY_LEFT_SHIFT)) {
		speedFactor = m_speedMod;
	}

	//camera looks toward negative z-axis so to go into view direction -> position -= forward
	if (glfwGetKey(inputWindow, GLFW_KEY_W)) {
		m_extrinsic.position -= m_extrinsic.forward * m_speed * deltaTime * speedFactor;
	}
	if (glfwGetKey(inputWindow, GLFW_KEY_S)) {
		m_extrinsic.position += m_extrinsic.forward * m_speed * deltaTime * speedFactor;
	}
	if (glfwGetKey(inputWindow, GLFW_KEY_D)) {
		m_extrinsic.position += m_extrinsic.right * m_speed * deltaTime * speedFactor;
	}
	if (glfwGetKey(inputWindow, GLFW_KEY_A)) {
		m_extrinsic.position -= m_extrinsic.right * m_speed * deltaTime * speedFactor;
	}
}