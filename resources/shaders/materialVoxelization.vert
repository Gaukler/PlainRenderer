#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec3 passPos;
layout(location = 1) out vec2 passUV;

layout(push_constant) uniform MatrixBlock {
	mat4 projection;
	mat4 model;
} translation;

void main(){
	passPos = (translation.model * vec4(inPos, 1.f)).xyz;
	passUV = inUV;
}