#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(location = 0) out vec2 passUV;
layout(location = 1) out vec4 passPos;
layout(location = 2) out vec4 passPosPrevious;
layout(location = 3) out vec3 passNormal;
layout(location = 4) out mat3 passTBN;

layout(push_constant) uniform MatrixBlock {
	mat4 mvp;
	mat4 previousMVP;
} translation;

void main(){
	gl_Position = translation.mvp * vec4(inPos, 1.f);
   
    passPos = gl_Position;
    passPosPrevious = translation.previousMVP * vec4(inPos, 1.f);
    passUV = inUV;
	passNormal = inNormal;

	vec3 T = normalize(inTangent);
    vec3 N = normalize(inNormal);
    vec3 B = normalize(inBitangent);

	passTBN = mat3(T, B, N);
}