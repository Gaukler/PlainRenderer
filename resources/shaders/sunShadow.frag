#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 passUV;

layout(set=2, binding = 0) uniform sampler colorSampler;
layout(set=2, binding = 3) uniform texture2D colorTexture;

void main(){
    float alpha = texture(sampler2D(colorTexture, colorSampler), passUV).a;
    if(alpha < 0.5f){
        discard;
    }
}