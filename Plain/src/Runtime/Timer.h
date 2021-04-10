#pragma once
#include "pch.h"

class Timer {
public:
    static void markNewFrame();
    static double getDeltaTime();
    static float getDeltaTimeFloat();
    static double getTime();
    static float getTimeFloat();
private:
    static double m_deltaTime;
    static double m_lastFrameTime;
    static float m_deltaTimeFloat;
};