#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "global.inc" 

layout(location = 0) in vec3 inPos;
layout(location = 0) out vec3 passPos;

void main(){
    gl_Position = g_viewProjection * vec4(inPos, 0.f);
    gl_Position.z = 0.f;
    passPos = inPos.xyz;
}