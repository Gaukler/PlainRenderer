#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;

layout(set=1, binding = 0, std430) buffer transformBuffer{
	mat4 transforms[];
};

layout(push_constant) uniform MatrixBlock {
	uint transformIndex;
};

void main(){
	gl_Position = transforms[transformIndex] * vec4(inPos, 1.f);
}