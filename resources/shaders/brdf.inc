float D_GGX(const float NoH, const float r){
	float roughness = r;
	float PI = 3.1415;
	float a = NoH * roughness;
    float k = roughness / (1.0 - NoH * NoH + a * a);
    return k * k * (1.0 / PI);
}

/*
geometric occlusion function
*/
float G_GGX(const float r, const float XoN){
	const float r_2 = r * r;
	const float nom = XoN * 2.f;
	const float denom = XoN + sqrt(r_2 + (1.f - r_2) * XoN * XoN);
	return nom / denom;
}

/*
visibility function
combines G with denominator
uses height correlated Smith
*/
float Visibility(const float NoV, const float NoL, const float r){
	const float r_2 = r * r;
	const float v1 = NoL * sqrt(NoV * NoV * (1.f - r_2) + r_2);
	const float v2 = NoV * sqrt(NoL * NoL * (1.f - r_2) + r_2);
	return 0.5f / (v1 + v2);
}

/*
combination of viewer occlusion and facet shadowing
uncorrelated smith
*/
float G_Smith(const float r, const float NoV, const float NoL){
	return G_GGX(r, NoV) * G_GGX(r, NoL);
}

vec3 F_Schlick(vec3 f0, vec3 f90, float VoH){
	return f0 + (f90 - f0) * pow(1.f - VoH, 5.f);
}