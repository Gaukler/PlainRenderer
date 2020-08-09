#pragma once
#include "pch.h"

class Timer {
public:
	static Timer& getReference();
	void markNewFrame();
	double getDeltaTime();
	float getDeltaTimeFloat();
	double getTime();
    float getTimeFloat();
private:
	Timer();
	Timer(Timer const&) = delete;
	void operator=(Timer const&) = delete;
	double m_deltaTime = 0.0;
	double m_lastFrameTime = 0.0;
	float m_deltaTimeFloat = 0.f;
	float padding = 0.f;
};