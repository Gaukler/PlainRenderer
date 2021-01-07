#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "global.inc"
#include "sky.inc"
#include "dither.inc"

layout(set=1, binding = 0) uniform texture2D skyLut;

layout(location = 0) in vec3 passPos;
layout(location = 0) out vec3 color;

void main(){     
    vec3 V = normalize(passPos); //from camera to sky
    color = sampleSkyLut(V, skyLut);

	//this isn't 'correct' dithering as it's written to a R11G11B10 render target
	//however it fixes the sky banding without introducing visible noise
	//banding might come from quantization errors, but might also be caused by limited LUT resolution or sample counts
	color = ditherRGB8(color, ivec2(gl_FragCoord.xy * g_screenResolution));
}