#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "global.inc"
#include "volume.inc"
#include "colorConversion.inc"
#include "materialVoxelization.inc"

layout(set=1, binding = 0, rgba8) uniform image3D materialVoxelImage;
layout(set=1, binding = 1, std140) uniform sdfVolumeData{
    vec4 sdfVolumeExtends;
    vec4 sdfVolumeOffset;
};

layout(set=1, binding = 2, std430) buffer sdfVolumeData{
    MaterialCounter counters[];
};

layout(set=2, binding = 0) uniform texture2D[] textures;

layout(push_constant) uniform MatrixBlock {
	uint albedoTextureIndex;
	uint transformIndex;
};

layout(location = 0) in vec3 passPos;
layout(location = 1) in vec2 passUV;

layout(location = 0) out float dummyColor;

void main(){     
	dummyColor = 0.f;

	vec3 posNormalized = worldPositionToVolume(passPos, sdfVolumeOffset.xyz, sdfVolumeExtends.xyz);
	ivec3 texelIndex = ivec3(posNormalized * imageSize(materialVoxelImage));

	vec3 albedoTexel = texture(sampler2D(textures[albedoTextureIndex], g_sampler_anisotropicRepeat), passUV).rgb;
	vec3 albedo = sRGBToLinear(albedoTexel);
	uvec3 albedoUInt = uvec3(albedo * 255);

	ivec3 imageRes = imageSize(materialVoxelImage);
	uint indexFlat = flatten3DIndex(texelIndex, imageRes);

	//only writing to one voxel per fragment
	//searching for additional voxels in z-direction using triangle/box intersection would be more accurate
	//but is not needed
	atomicAdd(counters[indexFlat].r, albedoUInt.r);
	atomicAdd(counters[indexFlat].g, albedoUInt.g);
	atomicAdd(counters[indexFlat].b, albedoUInt.b);
	atomicAdd(counters[indexFlat].counter, 1);
}