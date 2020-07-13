#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(set=0, binding = 0, std140) uniform global{
	vec4 sunColor;
	vec4 sunDirection;
	vec4 ambient;
	mat4 lightMatrix;
	vec4 cameraPosition;
    float sunStrengthExposed;
    float skyStrengthExposed;
};

layout(set=1, binding = 0) uniform textureCube skyTexture;
layout(set=1, binding = 1) uniform sampler skySampler;

layout(location = 0) in vec3 passPos;
layout(location = 0) out vec3 color;

void main(){
	color = texture(samplerCube(skyTexture, skySampler), passPos).rgb;    
    color *= skyStrengthExposed;
}