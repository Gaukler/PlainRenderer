#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "global.inc" 

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(location = 0) out vec2 passUV;
layout(location = 1) out vec3 passPos;
layout(location = 2) out mat3 passTBN; 


layout(push_constant) uniform MatrixBlock {
	mat4 mvp;
	mat4 model;
} translation;

void main(){
	gl_Position = translation.mvp * vec4(inPos, 1.f);
	passUV = inUv;
	passPos = (translation.model * vec4(inPos, 1.f)).xyz;

	vec3 T = normalize(mat3(translation.model) * inTangent);
    vec3 N = normalize(mat3(translation.model) * inNormal);
    vec3 B = normalize(mat3(translation.model) * inBitangent);

	passTBN = mat3(T, B, N);
}