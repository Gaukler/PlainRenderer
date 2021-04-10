#ifndef GEOMETRIC_AA_INC
#define GEOMETRIC_AA_INC

float modifiedRoughnessGeometricAA(vec3 N, float r){
    //reference: "Improved Geometric Specular Antialiasing"
    float kappa = 0.18f; //threshold
    float pixelVariance = 0.5f;
    float pxVar2 = pixelVariance * pixelVariance;

    vec3 N_U = dFdxFine(N);
    vec3 N_V = dFdyFine(N);
    //squared lengths
    float lengthN_U2 = dot(N_U, N_U);
    float lengthN_V2 = dot(N_V, N_V);

    float variance = pxVar2 * (lengthN_V2 + lengthN_U2);    
    float kernelRoughness2 = min(2.f * variance, kappa);
    float rFiltered = clamp(sqrt(r * r + kernelRoughness2), 0.f, 1.f);
    return rFiltered;
}

#endif // #ifndef GEOMETRIC_AA_INC