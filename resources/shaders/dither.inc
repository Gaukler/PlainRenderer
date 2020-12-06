#include "noise.inc"

vec3 ditherRGB8(vec3 c, ivec2 uv){
	vec3 noise = hash32(uvec2(uv * g_time));
	noise += hash32(uvec2((uv + vec2(165, 1292))*g_time));
	noise -= 1.f;
	noise /= 255.f; //least significant of 8 bits
 	return c + noise;
}