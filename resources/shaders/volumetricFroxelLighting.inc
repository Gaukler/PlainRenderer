const float maxVolumetricLightingDepth = 30.f; 

struct VolumetricLightingSettings{
	vec3 windSampleOffset;
	float maxDistance;
	vec3 scatteringCoefficients;
	float absorptionCoefficient;
	float sampleOffset;	
	float baseDensity;
	float densityNoiseRange;
	float densityNoiseScale;
	float phaseFunctionG;
};

//adjusts strength of exponential distribution
//higher k results in more slices near the camera
float k = 3.f;
bool exponentialDepthDistribution = true;

float froxelUVToDepth(float uvZ, float maxDistance){
	if(exponentialDepthDistribution){
		float remaped = (exp(k*uvZ)-1)/(exp(k)-1);
		return remaped * maxDistance;
	}
	else{
		return uvZ * maxDistance;
	}
}

float depthToFroxelUVZ(float depth, float maxDistance){
	if(exponentialDepthDistribution){
		float linear = depth / maxDistance;
		return log(linear*(exp(k)-1)+1)/k;
	}
	else{
		return depth / maxDistance;
	}
}

vec4 volumeTextureLookup(vec2 screenUV, float depth, texture3D froxelTexture, float maxDistance){
	vec3 uv;
	uv.xy = screenUV;

	uv.z = depthToFroxelUVZ(depth, maxDistance);
	return texture(sampler3D(froxelTexture, g_sampler_linearClamp), uv);
}

vec3 applyInscatteringTransmittance(vec3 originalColor, vec4 inscatteringTransmittance){
	return originalColor * inscatteringTransmittance.a + inscatteringTransmittance.rgb;
}