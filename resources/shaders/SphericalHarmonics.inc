//reference "Stupid Spherical Harmonics (SH) Tricks", Appendix A2
vec4 directionToSH_L1(vec3 V){
	return normalize(vec4(
		1.f / (2 * sqrt(pi)),
		-sqrt(3.f) * V.y / (2.f * sqrt(pi)),
		 sqrt(3.f) * V.z / (2.f * sqrt(pi)),
		-sqrt(3.f) * V.x / (2.f * sqrt(pi))));
}