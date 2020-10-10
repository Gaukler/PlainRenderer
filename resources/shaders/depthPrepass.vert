#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 passUV;
layout(location = 1) out vec4 passPos;
layout(location = 2) out vec4 passPosPrevious;

layout(push_constant) uniform MatrixBlock {
	mat4 mvp;
	mat4 previousMVP;
} translation;

void main(){
	gl_Position = translation.mvp * vec4(inPos, 1.f);
    
    
    passPos = gl_Position;
    passPosPrevious = translation.previousMVP * vec4(inPos, 1.f);
    passUV = inUV;
}