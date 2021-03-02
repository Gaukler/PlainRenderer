const int maxSunShadowCascadeCount = 4;
float shadowSampleRadius = 0.03f; //world space

struct ShadowCascadeInfo{
	vec4 splits;
    mat4 lightMatrices[maxSunShadowCascadeCount];
	vec2 lightSpaceScale[maxSunShadowCascadeCount];
};

vec3 sunShadowCascadeDebugColors(int cascadeIndex){
	vec3 cascadeColors[4] = {
		vec3(1, 0, 0),
		vec3(0, 1, 0),
		vec3(0, 0, 1),
		vec3(1, 1, 0)
	};
	return cascadeColors[cascadeIndex];
}