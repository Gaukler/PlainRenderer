layout(set=0, binding = 0, std140) uniform global{
	vec4 g_sunColor;
	vec4 g_sunDirection;
	mat4 g_lightMatrix;
	vec4 g_cameraPosition;
    float g_sunStrength;
    float g_skyStrength;
    float g_exposureOffset;
    float g_exposureAdaptionSpeedEvPerSec;
    float g_deltaTime;
};