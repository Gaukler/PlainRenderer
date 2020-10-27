layout(set=0, binding = 0, std140) uniform global{
	vec4 g_sunDirection;
	vec4 g_cameraPosition;
    vec4 g_cameraRight;
    vec4 g_cameraUp;
    vec4 g_cameraForward;   
    vec2 g_currentFrameCameraJitter;
    vec2 g_previousFrameCameraJitter;
    float g_cameraTanFovHalf;
    float g_cameraAspectRatio;
    float g_nearPlane;
    float g_farPlane;
    float g_sunStrength;
    float g_skyStrength;
    float g_exposureOffset;
    float g_exposureAdaptionSpeedEvPerSec;
    float g_deltaTime;
};

float pi = 3.1415926535f;