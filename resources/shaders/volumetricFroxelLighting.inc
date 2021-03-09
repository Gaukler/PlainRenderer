const float maxVolumetricLightingDepth = 30.f; 

//adjusts strength of exponential distribution
//higher k results in more slices near the camera
float k = 3.f;
bool exponentialDepthDistribution = true;

float froxelUVToDepth(float uvZ){
	if(exponentialDepthDistribution){
		float remaped = (exp(k*uvZ)-1)/(exp(k)-1);
		return remaped * maxVolumetricLightingDepth;
	}
	else{
		return uvZ * maxVolumetricLightingDepth;
	}
}

float depthToFroxelUVZ(float depth){
	if(exponentialDepthDistribution){
		float linear = depth / maxVolumetricLightingDepth;
		return log(linear*(exp(k)-1)+1)/k;
	}
	else{
		return depth / maxVolumetricLightingDepth;
	}
}

vec4 volumeTextureLookup(vec2 screenUV, float depth, texture3D froxelTexture, vec2 lookupNoiseVector){
	vec3 uv;
	uv.xy = screenUV;
	uv.xy += lookupNoiseVector;

	uv.z = depthToFroxelUVZ(depth);
	return texture(sampler3D(froxelTexture, g_sampler_linearClamp), uv);
}

vec3 applyInscatteringTransmittance(vec3 originalColor, vec4 inscatteringTransmittance){
	return originalColor * inscatteringTransmittance.a + inscatteringTransmittance.rgb;
}