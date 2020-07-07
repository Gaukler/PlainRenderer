vec3 importanceSampleGGX(float thetaIn, float phiIn, float r, vec3 N){	
	float r_2 = r * r;
	float cosTheta = sqrt((1.f - thetaIn) / (1.f + (r_2 - 1.f) * thetaIn));
    float sinTheta = sqrt(1 - cosTheta * cosTheta);
	float phi = 2.f * 3.1415 * phiIn;
	vec3 sampleHemisphere = vec3(cos(phi)*sinTheta, sin(phi)*sinTheta, cosTheta);
	
	/*
	orient sample into world space
	*/
	vec3 up = abs(N.z) < 0.999 ? vec3(0.f, 0.f, 1.f) : vec3(1.f, 0.f, 0.f);
	vec3 tangent = normalize(cross(up, N));
	vec3 bitangent = cross(N, tangent);
	
	vec3 sampleWorld = vec3(0);
	sampleWorld += sampleHemisphere.x * tangent;
	sampleWorld += sampleHemisphere.y * bitangent;
	sampleWorld += sampleHemisphere.z * N;
	
	return sampleWorld;
}

float radicalInverse_VdC(uint bits) {
     bits = (bits << 16u) | (bits >> 16u);
     bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
     bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
     bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
     bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
     return float(bits) * 2.3283064365386963e-10; // / 0x100000000
 }
 
 vec2 hammersley2d(uint i, uint N) {
     return vec2(float(i)/float(N), radicalInverse_VdC(i));
 }