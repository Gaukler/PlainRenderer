#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "global.inc"

layout(location = 0) in vec2 passUV;

layout(set=2, binding = 0) uniform texture2D[] textures;

layout(push_constant) uniform MatrixBlock {
	uint albedoTextureIndex;
	uint transformIndex;
};

void main(){
    float alpha = texture(sampler2D(textures[albedoTextureIndex], g_sampler_anisotropicRepeat), passUV).a;
    if(alpha < 0.5f){
        discard;
    }
}