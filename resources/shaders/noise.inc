//from: "Next Generation Post Processing in Call Of Duty Advanced Warfare" slide page 123
//float interleavedGradientNoise(vec2 uv){
//    vec3 magic = vec3(0.06711056, 0.00583715, 62.9829189);
//    return fract(magic.z * fract(dot(uv, magic.xy)));
//}

//from: https://www.shadertoy.com/view/XdGfRR, by David Hoskins
//recommended in https://www.shadertoy.com/view/Xt3cDn, as a fast but low quality hash
//used here because many high quality hashes have int in our output which is less convenient to work with
//TODO: research higher quality options and impact on used techniques
vec3 hash32(vec2 q)
{
	uvec3 n = uvec3(ivec3(q.xyx)) * UI3;
	n = (n.x ^ n.y ^ n.z) * UI3;
	return vec3(n) * UIF;
}