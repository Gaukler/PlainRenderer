layout(set=0, binding = 0, std140) uniform global{
	vec4 g_sunDirection;
	vec4 g_cameraPosition;
    vec4 g_cameraRight;
    vec4 g_cameraUp;
    vec4 g_cameraForward;   
    vec2 g_currentFrameCameraJitter;
    vec2 g_previousFrameCameraJitter;
    ivec2 g_screenResolution;
    float g_cameraTanFovHalf;
    float g_cameraAspectRatio;
    float g_nearPlane;
    float g_farPlane;
    float g_sunStrength;
    float g_exposureOffset;
    float g_exposureAdaptionSpeedEvPerSec;
    float g_deltaTime;
    float g_time;
	float g_mipBias;
	bool g_cameraCut;
};

layout(set=0, binding = 1) uniform sampler g_sampler_anisotropicRepeat;
layout(set=0, binding = 2) uniform sampler g_sampler_nearestBlackBorder;
layout(set=0, binding = 3) uniform sampler g_sampler_linearRepeat;
layout(set=0, binding = 4) uniform sampler g_sampler_linearClamp;
layout(set=0, binding = 5) uniform sampler g_sampler_nearestClamp;
layout(set=0, binding = 6) uniform sampler g_sampler_linearWhiteBorder;

layout(set=0, binding = 7) uniform texture2D g_noiseTexture;

float pi = 3.1415926535f;