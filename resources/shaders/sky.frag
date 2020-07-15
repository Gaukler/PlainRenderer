#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "global.inc"

layout(set=1, binding = 0) uniform textureCube skyTexture;
layout(set=1, binding = 1) uniform sampler skySampler;

layout(set=1, binding = 2, std430) buffer lightBuffer{
    float previousFrameExposure;
    float sunStrengthExposed;
    float skyStrengthExposed;
};

layout(location = 0) in vec3 passPos;
layout(location = 0) out vec3 color;

void main(){
	color = texture(samplerCube(skyTexture, skySampler), passPos).rgb;    
    color *= skyStrengthExposed;
}