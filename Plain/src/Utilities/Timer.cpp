#include "pch.h"
#include "Timer.h"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif

#include <GLFW/glfw3.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif


Timer& Timer::getReference() {
	static Timer timer;
	return timer;
}

void Timer::markNewFrame() {
	double currentTime = getTime();
	m_deltaTime = currentTime - m_lastFrameTime;
	if (m_deltaTime > 0.3f) { //workaround for paused application
		m_deltaTime = 0.016f;
	}
	m_deltaTimeFloat = static_cast<float>(m_deltaTime);
	m_lastFrameTime = currentTime;
}

double Timer::getDeltaTime() {
	return m_deltaTime;
}

double Timer::getTime() {
	return glfwGetTime();
}

float Timer::getTimeFloat() {
    return glfwGetTime();
}

float Timer::getDeltaTimeFloat() {
	return m_deltaTimeFloat;
}

Timer::Timer() : m_lastFrameTime(getTime()) {}