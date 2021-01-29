#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "shadowCascadeConstants.inc"

layout(constant_id = 0) const uint cascadeIndex = 0;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUv;

layout(location = 0) out vec2 passUV;

layout(push_constant) uniform MatrixBlock {
	uint albedoTextureIndex;
	uint transformIndex;
};

layout(set=1, binding = 0, std430) buffer sunShadowInfo{
    vec4 cascadeSplits;
    mat4 lightMatrix[cascadeCount];
};

layout(set=1, binding = 1, std430) buffer transformBuffer{
	mat4 transforms[];
};

void main(){
	gl_Position = lightMatrix[cascadeIndex] * transforms[transformIndex] * vec4(inPos, 1.f);
    passUV = inUv;
}