float phaseGreenstein(float VoL, float g){
    return (1.f - g * g) / (4.f * pi * pow(1.f + g * g - 2.f * g * VoL, 1.5f));
}

//approximates greenstein, has sign error
float phaseSchlick(float VoL, float g){
    float k = 1.55f * g - 0.55f * g * g * g;
    return (1.f - k * k) / (4.f * pi * pow(1.f + k * VoL, 2.f));
}

float phaseRayleigh(float VoL){
    return 3.f / (16.f * pi) * (1.f + VoL * VoL);
}

float cornetteShanksPhase(float VoL, float g){
    float nominator = 3.f / (8.f * pi) * (1 - g * g) * (1 + VoL * VoL);
    float denominator = (2 + g * g) * pow(1 + g * g - 2 * g * VoL, 1.5f);
    return nominator / denominator;
}

//reference: "Physically Based and Unified Volumetric Rendering in Frostbite", page 29
vec3 integrateInscattering(vec3 inscattering, vec3 extinctionCoefficients, float length){
    return (inscattering - inscattering * exp(-extinctionCoefficients * length)) / extinctionCoefficients;
}