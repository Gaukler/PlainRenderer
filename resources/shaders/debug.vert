#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "global.inc" 

layout(location = 0) in vec3 inPos;

layout(push_constant) uniform MatrixBlock {
	uint transformIndex;
};

layout(set=1, binding = 0, std430) buffer transformBuffer{
	mat4 transforms[];
};

void main(){
	gl_Position = transforms[transformIndex] * vec4(inPos, 1.f);
}