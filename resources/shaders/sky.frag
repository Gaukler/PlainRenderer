#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "global.inc"
#include "sky.inc"

layout(set=1, binding = 0) uniform texture2D skyLut;
layout(set=1, binding = 1) uniform sampler skySampler;

layout(set=1, binding = 2, std430) buffer lightBuffer{
    float previousFrameExposure;
    float sunStrengthExposed;
    float skyStrengthExposed;
};

layout(set=1, binding = 3, std140) uniform atmosphereSettings{
    vec3 scatteringRayleighGround;
    float earthRadius;
    vec3 extinctionRayleighGround;
    float atmosphereHeight;
    vec3 ozoneExtinction;
    float scatteringMieGround;
    float extinctionMieGround;
};

layout(location = 0) in vec3 passPos;
layout(location = 0) out vec3 color;

float phaseGreenstein(float VoL, float g){
    return (1.f - g * g) / (4.f * 3.1415f * pow(1.f + g * g - 2.f * g * VoL, 1.5f));
}
    
//approximates greenstein, has sign error
float phaseSchlick(float VoL, float g){
    float k = 1.55f * g - 0.55f * g * g * g;
    return (1.f - k * k) / (4.f * 3.1415 * pow(1.f + k * VoL, 2.f));
}

float phaseRayleigh(float VoL){
    return 3.f / (16.f * 3.1415f) * (1.f + VoL * VoL);
}

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