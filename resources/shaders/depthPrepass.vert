#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "MainPassMatrices.inc"

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

layout(set=1, binding = 0, std430) buffer transformBuffer{
	MainPassMatrices transforms[];
};

layout(push_constant) uniform MatrixBlock {
	uint albedoTextureIndex;
	uint normalTextureIndex;
	uint specularTextureIndex;
	uint transformIndex;
};

void main(){
	gl_Position = transforms[transformIndex].mvp * vec4(inPos, 1.f);
   
    passPos = gl_Position;
    passPosPrevious = transforms[transformIndex].mvpPrevious * vec4(inPos, 1.f);
    passUV = inUV;

	vec3 T = normalize(mat3(transforms[transformIndex].model) * inTangent);
    vec3 N = normalize(mat3(transforms[transformIndex].model) * inNormal);
    vec3 B = normalize(mat3(transforms[transformIndex].model) * inBitangent);

	passTBN = mat3(T, B, N);
}