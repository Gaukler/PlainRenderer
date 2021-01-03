#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "global.inc"
#include "volume.inc"
#include "colorConversion.inc"

layout(set=1, binding = 0, rgba8) uniform image3D materialVoxelImage;
layout(set=1, binding = 1, std140) uniform sdfVolumeData{
    vec4 sdfVolumeExtends;
    vec4 sdfVolumeOffset;
};

layout(set=2, binding = 0) uniform texture2D colorTexture;

layout(location = 0) in vec3 passPos;
layout(location = 1) in vec2 passUV;

layout(location = 0) out float dummyColor;

void main(){     
	dummyColor = 0.f;

	vec3 posNormalized = worldPositionToVolume(passPos, sdfVolumeOffset.xyz, sdfVolumeExtends.xyz);
	ivec3 texelIndex = ivec3(posNormalized * imageSize(materialVoxelImage));

	vec3 albedoTexel = texture(sampler2D(colorTexture, g_sampler_anisotropicRepeat), passUV, g_mipBias).rgb;
	vec3 albedo = sRGBToLinear(albedoTexel);

	imageStore(materialVoxelImage, texelIndex, vec4(albedo, 1.f));
}