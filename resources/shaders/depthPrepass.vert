#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;


layout(push_constant) uniform MatrixBlock {
	mat4 mvp;
	mat4 model;
} translation;

void main(){
	gl_Position = translation.mvp * vec4(inPos, 1.f);
}