#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "global.inc" 

layout(location = 0) in vec2 passUV;
layout(location = 1) in vec4 passPos;
layout(location = 2) in vec4 passPosPrevious;
layout(location = 3) in vec3 passNormal;
layout(location = 4) in mat3 passTBN;

layout(set=2, binding = 0) uniform texture2D colorTexture;
layout(set=2, binding = 1) uniform texture2D normalTexture;

layout(location = 0) out vec2 motion;
layout(location = 1) out vec3 normal;

void main(){
    float alpha = texture(sampler2D(colorTexture, g_sampler_anisotropicRepeat), passUV, g_mipBias).a;
    if(alpha < 0.5f){
        discard;
    }
    
    //computing per pixel motion vectors
    //reference: "Temporal Antialiasing in Uncharted 4"
    vec2 ndcCurrent  = passPos.xy           / passPos.w;
    vec2 ndcPrevious = passPosPrevious.xy   / passPosPrevious.w;
    
    ndcCurrent  += g_currentFrameCameraJitter;
    ndcPrevious += g_previousFrameCameraJitter;
    
    motion = (ndcPrevious - ndcCurrent) * vec2(0.5f, 0.5f);

	vec2 normalTexel 		= texture(sampler2D(normalTexture, 		g_sampler_anisotropicRepeat), passUV, g_mipBias).rg;
    vec3 normalTexelReconstructed = vec3(normalTexel, sqrt(1.f - normalTexel.x * normalTexel.x + normalTexel.y + normalTexel.y));
    normalTexelReconstructed = normalTexelReconstructed * 2.f - 1.f;

	normal = normalize(passTBN * normalTexelReconstructed) * 0.5 + 0.5; 

	normal = normalize(passNormal) * 0.5 + 0.5;
}