#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(location = 0) out vec2 passUV;
layout(location = 1) out vec3 passNormal;
layout(location = 2) out vec4 passPos;
layout(location = 3) out mat3 passTBN;

layout(push_constant) uniform MatrixBlock {
	mat4 mvp;
	mat4 model;
} translation;

void main(){
	gl_Position = translation.mvp * vec4(inPos, 1.f);
	passUV = inUv;
	passNormal = inNormal;
	passPos = translation.model * vec4(inPos, 1.f);
	
	vec3 T = mat3(translation.model) * inTangent;
	vec3 B = mat3(translation.model) * inBitangent;
	vec3 N = mat3(translation.model) * inNormal;
	passTBN = mat3(T, B, N);
}