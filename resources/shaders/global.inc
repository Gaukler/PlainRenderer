layout(set=0, binding = 0, std140) uniform global{
	vec4 g_sunColor;
	vec4 g_sunDirection;
	mat4 g_lightMatrix;
	vec4 g_cameraPosition;
    float g_sunStrength;
    float g_skyStrength;
    float exposureOffset;
};