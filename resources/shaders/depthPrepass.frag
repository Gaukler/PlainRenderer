#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "global.inc" 

layout(location = 0) in vec2 passUV;
layout(location = 1) in vec4 passPos;
layout(location = 2) in vec4 passPosPrevious;

layout(set=2, binding = 0) uniform sampler colorSampler;
layout(set=2, binding = 3) uniform texture2D colorTexture;

layout(location = 0) out vec2 motion;

void main(){
    float alpha = texture(sampler2D(colorTexture, colorSampler), passUV).a;
    if(alpha < 0.5f){
        discard;
    }
    
    //computing per pixel motion vectors
    //reference: "Temporal Antialiasing in Uncharted 4"
    vec2 ndcCurrent  = passPos.xy           / passPos.w;
    vec2 ndcPrevious = passPosPrevious.xy   / passPosPrevious.w;
    
    ndcCurrent  -= g_currentFrameCameraJitter;
    ndcPrevious -= g_previousFrameCameraJitter;
    
    motion = (ndcPrevious - ndcCurrent) * vec2(0.5f, 0.5f);
    //motion = vec2(0);
}