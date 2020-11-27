#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "global.inc"
#include "sky.inc"
#include "lightBuffer.inc"

layout(set=1, binding = 0, std430) buffer lightStorageBuffer{
    LightBuffer lightBuffer;
}; 

layout(set=1, binding = 1) uniform texture2D transmissionLut;

layout(location = 0) in vec2 passQuadPos;
layout(location = 1) in vec3 passWorldPos;

layout(location = 0) out vec4 color;

//from: "Wavelength dependency of the Solar limb darkening" equation(1)
//coefficients picked from table 2 at wavelenghts corresponding to RGB
//see also: "Physically Based Sky, Atmosphereand Cloud Rendering in Frostbite" section 4.1.3
vec3 limbDarkening(float distanceToCenterSquared){
    vec3 coefficients = vec3(0.482, 0.511, 0.643);
    float mu = sqrt(1 - distanceToCenterSquared);
    return pow(vec3(mu), coefficients); //equation with u = 1 and simplified
}

void main(){
    vec2 posCentered = passQuadPos;
    float distanceFromCenter = dot(posCentered, posCentered);
    if(distanceFromCenter > 1.f){
        discard;
    }
    float bias = 0.002; //corresponds to bias in skyLut.comp
    vec3 V = normalize(passWorldPos + vec3(0, bias, 0));
    vec2 lutUV = computeLutUV(0, 100, vec3(0, -1, 0), V);
    vec3 transmission = texture(sampler2D(transmissionLut, g_sampler_linearClamp), lutUV).rgb;
    color.rgb = lightBuffer.sunStrengthExposed * transmission * limbDarkening(distanceFromCenter);
	//soft alpha blend at edge
	color.a = 1.f - distanceFromCenter;
	color.a *= color.a;
}