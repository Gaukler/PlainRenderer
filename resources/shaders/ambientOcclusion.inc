//from: "Practical Realtime Strategies for Accurate Indirect Occlusion", section 5
vec3 AOIndirectBounceApproximation(vec3 albedo, float ao){
    vec3 a = 2.0404 * albedo - 0.3324;
    vec3 b = 4.7951 * albedo - 0.6417; 
    vec3 c = 2.7552 * albedo + 0.6903;
    return a * ao * ao * ao - b * ao * ao + c * ao;
}