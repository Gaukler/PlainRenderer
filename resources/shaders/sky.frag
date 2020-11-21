#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "global.inc"
#include "sky.inc"

layout(set=1, binding = 0) uniform texture2D skyLut;

layout(location = 0) in vec3 passPos;
layout(location = 0) out vec3 color;

void main(){     
    vec3 V = normalize(passPos); //from camera to sky
    vec2 uv = toSkyLut(V);
    uv.y = clamp(uv.y, 0.005f, 0.995); //avoid wrapping artifact at extreme angles
    color = texture(sampler2D(skyLut, g_sampler_linearRepeat), uv).rgb;
}