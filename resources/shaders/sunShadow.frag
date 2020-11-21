#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "global.inc"

layout(location = 0) in vec2 passUV;

layout(set=2, binding = 0) uniform texture2D colorTexture;

void main(){
    float alpha = texture(sampler2D(colorTexture, g_sampler_anisotropicRepeat), passUV).a;
    if(alpha < 0.5f){
        discard;
    }
}