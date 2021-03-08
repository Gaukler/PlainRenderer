const float maxVolumetricLightingDepth = 30.f; 

vec4 volumeTextureLookup(vec2 screenUV, float depth, texture3D froxelTexture, vec2 lookupNoiseVector){
	vec3 uv;
	uv.xy = screenUV;
	uv.xy += lookupNoiseVector;

	uv.z = clamp(depth / maxVolumetricLightingDepth, 0.f, 1.f);
	return texture(sampler3D(froxelTexture, g_sampler_linearClamp), uv);
}

vec3 applyInscatteringTransmittance(vec3 originalColor, vec4 inscatteringTransmittance){
	return originalColor * inscatteringTransmittance.a + inscatteringTransmittance.rgb;
}