#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "global.inc"
#include "sky.inc"

layout(set=1, binding = 0) uniform texture2D skyLut;
layout(set=1, binding = 1) uniform sampler skySampler;

layout(location = 0) in vec3 passPos;
layout(location = 0) out vec3 color;

void main(){     
    vec3 V = normalize(passPos); //from camera to sky
    vec2 uv = toSkyLut(V);
     
    color = texture(sampler2D(skyLut, skySampler), uv).rgb;
    
    vec3 L = g_sunDirection.xyz;
    float VoL = dot(V, L);
    float sunDisk = pow(max(VoL, 0), 1000);
    if(sunDisk > 0.99){
        color += vec3(10);
    }
}