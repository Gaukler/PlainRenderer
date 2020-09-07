#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in float passHeight;

layout(location = 0) out float height;

void main(){
    height = -passHeight; //y points down, so needs to be negated for height
}