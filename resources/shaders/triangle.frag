#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "colorConversion.inc"
#include "brdf.inc"
#include "global.inc" 
#include "GeometricAA.inc"
#include "shadowCascadeConstants.inc"
#include "linearDepth.inc"

/*
specialisation constants
*/

//0: lambert
//1: disney
//2: CoD WWII
//3: Titanfall 2
layout(constant_id = 0) const int diffuseBRDF = 0;

//0: McAuley
//1: simplified
//2: scaled GGX lobe
//3: none
layout(constant_id = 1) const int directMultiscatterBRDF = 0;

layout(constant_id = 2) const bool indirectMultiscatterBRDF = false;

//0: disabled
//1: enabled
layout(constant_id = 3) const bool geometricAA = false;

layout(constant_id = 4) const uint specularProbeMipCount = 0;
layout(constant_id = 5) const float TextureLoDBias = 0;
layout(constant_id = 6) const bool useSkyOcclusion = true;

layout(set=1, binding = 0) uniform sampler depthSampler;

layout(set=1, binding = 1) 	uniform textureCube	diffuseProbe;
layout(set=1, binding = 2) 	uniform sampler 	cubeSampler;

layout(set=1, binding = 3) 	uniform texture2D 	brdfLutTexture;
layout(set=1, binding = 4) 	uniform textureCube	specularProbe;
layout(set=1, binding = 5) 	uniform sampler 	specularProbeSampler;
layout(set=1, binding = 6) 	uniform sampler 	lutSampler;

layout(set=1, binding = 7, std430) buffer lightBuffer{
    float previousFrameExposure;
    float sunStrengthExposed;
    float skyStrengthExposed;
};

layout(set=1, binding = 8, std430) buffer sunShadowInfo{
    vec4 cascadeSplits;
    mat4 lightMatrices[cascadeCount];
};

//bindings currently limit cascade count to 6, however should always be enough 4 anyway
layout(set=1, binding = 9)  uniform texture2D shadowMapCascade0;
layout(set=1, binding = 10) uniform texture2D shadowMapCascade1;
layout(set=1, binding = 11) uniform texture2D shadowMapCascade2;
layout(set=1, binding = 12) uniform texture2D shadowMapCascade3;

layout(set=1, binding = 13) uniform texture3D skyOcclusionVolume;

layout(set=1, binding = 14, std140) uniform occlusionData{
	mat4 skyShadowMatrix;
    vec4 occlusionVolumeExtends;
    vec4 sampleDirection;
    vec4 occlusionVolumeOffset;
    ivec4 texelMotionGathering;
    ivec4 texelMotionRendering;
    ivec4 texelMotionBlending;
    float weight;
};

layout(set=1, binding = 15) uniform sampler occlusionSampler;

layout(set=2, binding = 0) uniform sampler colorSampler;
layout(set=2, binding = 1) uniform sampler normalSampler;
layout(set=2, binding = 2) uniform sampler specularSampler;

layout(set=2, binding = 3) uniform texture2D colorTexture;
layout(set=2, binding = 4) uniform texture2D normalTexture;
layout(set=2, binding = 5) uniform texture2D specularTexture;

layout(location = 0) in vec2 passUV;
layout(location = 1) in vec3 passPos;
layout(location = 2) in mat3 passTBN; 

layout(location = 0) out vec3 color;

float shadowTest(texture2D shadowMap, vec2 uv, float actualDepth){
    vec4 depthTexels = textureGather(sampler2D(shadowMap, depthSampler), uv, 0);
    
    vec4 tests;
    tests.r = float(actualDepth <= depthTexels.r);
    tests.g = float(actualDepth <= depthTexels.g);
    tests.b = float(actualDepth <= depthTexels.b);
    tests.a = float(actualDepth <= depthTexels.a);
    
    ivec2 shadowMapRes = textureSize(sampler2D(shadowMap, depthSampler), 0);
    vec2 sampleImageCoordinates = vec2(shadowMapRes) * uv + 0.502f;
    vec2 coordinatesFloored = floor(sampleImageCoordinates);
    vec2 interpolation = sampleImageCoordinates - coordinatesFloored;
    
	float interpolationLeft = mix(tests.b, tests.g, interpolation.y);
    float interpolationRight  = mix(tests.a, tests.r, interpolation.y);
    
    float shadow = mix(interpolationRight, interpolationLeft, interpolation.x);

    return shadow;
}

float calcShadow(vec3 pos, float LoV, texture2D shadowMap, mat4 lightMatrix){
    //normal used for bias
    //referenc: http://c0de517e.blogspot.com/2011/05/shadowmap-bias-notes.html
    float biasMin = 0.001f;
    float biasMax = 0.03f;
    float bias = mix(biasMax, biasMin, LoV);
    
    //we don't want vector from normal map as this is a geometric operation
    vec3 N = normalize(passTBN[2]);
    vec3 offsetPos = pos + N * bias;
    
	vec4 posLightSpace = lightMatrix * vec4(offsetPos, 1.f);
	posLightSpace /= posLightSpace.w;
	posLightSpace.xy = posLightSpace.xy * 0.5f + 0.5f;
	float actualDepth = clamp(posLightSpace.z, 0.f, 1.f);
    
    vec2 texelSize = vec2(1.f) / textureSize(sampler2D(shadowMap, depthSampler), 0);
    float radius = 1.f;
    
    float shadow = 0;
    shadow += shadowTest(shadowMap, posLightSpace.xy, actualDepth);
    shadow += shadowTest(shadowMap, posLightSpace.xy + vec2( 1,  1) * texelSize * radius, actualDepth);
    shadow += shadowTest(shadowMap, posLightSpace.xy + vec2( 1, -1) * texelSize * radius, actualDepth);
    shadow += shadowTest(shadowMap, posLightSpace.xy + vec2(-1,  1) * texelSize * radius, actualDepth);
    shadow += shadowTest(shadowMap, posLightSpace.xy + vec2(-1, -1) * texelSize * radius, actualDepth);
    shadow += shadowTest(shadowMap, posLightSpace.xy + vec2( 1,  0) * texelSize * radius, actualDepth);
    shadow += shadowTest(shadowMap, posLightSpace.xy + vec2(-1,  0) * texelSize * radius, actualDepth);
    shadow += shadowTest(shadowMap, posLightSpace.xy + vec2( 0,  1) * texelSize * radius, actualDepth);
    shadow += shadowTest(shadowMap, posLightSpace.xy + vec2( 0, -1) * texelSize * radius, actualDepth);
    
    return shadow / 9.f;
}

//mathematical fit from: "A Journey Through Implementing Multiscattering BRDFs & Area Lights"
float EnergyAverage(float roughness){
    float smoothness = 1.f - pow(roughness, 0.25f);
    float r = -0.0761947f - 0.383026f * smoothness;
          r = 1.04997f + smoothness * r;
          r = 0.409255f + smoothness * r;
    return min(0.999f, r);

}

void main(){
	vec3 albedoTexel 		= texture(sampler2D(colorTexture, 		colorSampler), 		passUV, TextureLoDBias).rgb;
	vec3 specularTexel 		= texture(sampler2D(specularTexture, 	specularSampler), 	passUV, TextureLoDBias).rgb;
	vec2 normalTexel 		= texture(sampler2D(normalTexture, 		normalSampler), 	passUV, TextureLoDBias).rg;
    vec3 normalTexelReconstructed = vec3(normalTexel, sqrt(1.f - normalTexel.x * normalTexel.x + normalTexel.y + normalTexel.y));
    normalTexelReconstructed = normalTexelReconstructed * 2.f - 1.f;
    
    float microAO = specularTexel.r; //not used
    float metalic = specularTexel.b;
    float r = specularTexel.g;
    
    r = max(r * r, 0.0045f);
    vec3 albedo = sRGBToLinear(albedoTexel);

	vec3 N = normalize(passTBN * normalTexelReconstructed);   
	vec3 L = normalize(g_sunDirection.xyz);
    
	vec3 V = g_cameraPosition.xyz - passPos;
    float pixelDistance = dot(V, g_cameraForward.xyz); 
    V = normalize(V);
    
	vec3 H = normalize(V + L);
	vec3 R = reflect(-V, N);
    
    if(geometricAA){
        r = modifiedRoughnessGeometricAA(N, r);
    }
    
	const float NoH = max(dot(N, H), 0);
	const float NoL = max(dot(N, L), 0);
	const float NoV = abs(dot(N, V));
	const float VoH = abs(dot(V, H));
    const float LoV = max(dot(L, V), 0.f);
	
	const vec3 f0 = mix(vec3(0.04f), albedo, metalic);
    
    //sun light
    float sunShadow;
    int cascadeIndex = 0;
    for(int cascade = 0; cascade < cascadeCount - 1; cascade++){
        cascadeIndex += int(pixelDistance >= cascadeSplits[cascade]);
    }

    if(cascadeIndex == 0){
        sunShadow = calcShadow(passPos, LoV, shadowMapCascade0, lightMatrices[cascadeIndex]);
    }
    else if(cascadeIndex == 1){
        sunShadow = calcShadow(passPos, LoV, shadowMapCascade1, lightMatrices[cascadeIndex]);
    }
    else if(cascadeIndex == 2){
        sunShadow = calcShadow(passPos, LoV, shadowMapCascade2, lightMatrices[cascadeIndex]);
    }
    else if(cascadeIndex == 3){
        sunShadow = calcShadow(passPos, LoV, shadowMapCascade3, lightMatrices[cascadeIndex]);
    }
	vec3 directLighting = max(dot(N, L), 0.f) * sunShadow * g_sunColor.rgb;

    //direct diffuse
    vec3 diffuseColor = (1.f - metalic) * albedo;
    
    vec3 diffuseDirect;
    vec3 diffuseBRDFIntegral = vec3(1.f);
    
    vec3 brdfLut = texture(sampler2D(brdfLutTexture, lutSampler), vec2(r, NoV)).rgb;
    
    //lambert
    if(diffuseBRDF == 0){
        diffuseDirect = diffuseColor / 3.1415f * directLighting;
        //even though lambert is constant the in/out fresnel terms need to be taken into account, these are integrated into the LUT
        diffuseBRDFIntegral = vec3(brdfLut.z);
    }
    //disney diffuse
	else if (diffuseBRDF == 1){
        vec3 disneyDiffuse = DisneyDiffuse(diffuseColor, NoL, VoH, NoV, r);
        diffuseDirect = disneyDiffuse * directLighting;
        diffuseBRDFIntegral = vec3(brdfLut.z);
    }
    //Cod WWII diffuse BRDF
    else if (diffuseBRDF == 2){
        vec3 fr = CoDWWIIDiffuse(diffuseColor, NoL, VoH, NoV, NoH, r);
        diffuseDirect = fr * directLighting;      
        diffuseBRDFIntegral = vec3(brdfLut.z);        
    }
    //titanfall 2 diffuse from gdc presentation
    else if (diffuseBRDF == 3){
        vec3 diffuseTitanfall2 = Titanfall2Diffuse(diffuseColor, NoL, LoV, NoV, NoH, r);
        diffuseDirect = diffuseTitanfall2 * directLighting;
        
        //"multi" part of the BRDF has non-linear dependence on albedo, so it can't be integrated into the LUT
        //however the expression is very simple and can be integrated analytically
        float multiIntegral = 0.1159f * r * 3.1415 * 2.f;
        //in/out fresnel has to be taken into account for multi part as well
        //metals have no diffuse part, non-metals all use F0 of 0.04
        vec3 F0Diffuse = vec3(0.04f);
        //out fresnel only depends on NoV which is constant
        multiIntegral *= (1.f - F_Schlick(F0Diffuse, vec3(1.f), NoV).r);
        //in fresnel can be integrated numerically and multiplied onto the rest as the rest is constant
        multiIntegral *= 0.94291f;
        
        diffuseBRDFIntegral = min(vec3(brdfLut.z + diffuseColor * multiIntegral), vec3(1.f));
    }
    //need to account for incoming and outgoing fresnel effect
    //see: https://seblagarde.wordpress.com/2011/08/17/hello-world/#comment-2405
    diffuseDirect *= (1.f - F_Schlick(f0, vec3(1.f), NoV)) * (1.f - F_Schlick(f0, vec3(1.f), NoL));
	
    //indirect specular
    vec3 lightingIndirect;
    float probeLoD = specularProbeMipCount * r;
    vec3 environmentSample = textureLod(samplerCube(specularProbe, specularProbeSampler), R, probeLoD).rgb;
    vec3 irradiance = texture(samplerCube(diffuseProbe, cubeSampler), N).rgb;
    vec3 fresnelAverage = f0 + (1-f0) / 21.f;
    
    
    if(indirectMultiscatterBRDF){
        //multi scattering for IBL from "A Multiple-Scattering Microfacet Model for Real-Time Image-based Lighting"
        vec3 singleScattering = (brdfLut.x * f0 + brdfLut.y); //includes fresnel and energy
        
        float energySingleScattering = brdfLut.x + brdfLut.y;
        float energyMultiScattering = 1 - energySingleScattering;
        vec3 fresnelMultiScattering = singleScattering * fresnelAverage / (1.f - (1.f - energySingleScattering) * fresnelAverage);
        vec3 multiScattering = energyMultiScattering * fresnelMultiScattering;
        
        vec3 energyDiffuseMultiScattering = 1.f - (singleScattering + multiScattering);
        vec3 diffuseCorrection = diffuseColor * energyDiffuseMultiScattering;
        
        lightingIndirect = singleScattering * environmentSample + (multiScattering + diffuseCorrection * diffuseBRDFIntegral) * irradiance;
    }
    else {
        //single scattering only
        vec3 singleScattering = (brdfLut.x * f0 + brdfLut.y);
        lightingIndirect = singleScattering * environmentSample + irradiance * diffuseColor * diffuseBRDFIntegral;
    }
    
    //direct specular
	const float D = D_GGX(NoH, r);
	const float Vis = Visibility(NoV, NoL, r);
	const vec3 F = F_Schlick(f0, vec3(1.f), VoH);
    vec3 singleScatteringLobe = D * Vis * F;
    
    float energyOutgoing = brdfLut.x + brdfLut.y;
    
    /*
    multiscattering formulation from "A Journey Through Implementing Multiscattering BRDFs & Area Lights"
    */     
    vec3 multiScatteringLobe;
    if(directMultiscatterBRDF == 0){
        float energyAverage = EnergyAverage(r);
    
        vec2 brdfLutIncoming = texture(sampler2D(brdfLutTexture, lutSampler), vec2(r, NoL)).rg;
        float energyIncoming = brdfLutIncoming.x + brdfLutIncoming.y;
        
        float multiScatteringLobeFloat = (1.f - energyIncoming) * (1.f - energyOutgoing) / (3.1415f * (1.f - energyAverage));
        vec3 multiScatteringScaling = (fresnelAverage * fresnelAverage * energyAverage) / (1.f - fresnelAverage * (1.f - energyAverage));
        
        multiScatteringLobe = multiScatteringLobeFloat * multiScatteringScaling;
    }
    //this is the above but approximating E_avg = E_o, simplifying the equation
    else if(directMultiscatterBRDF == 1){
        multiScatteringLobe = vec3((1.f - energyOutgoing) / 3.1415f);
        vec3 multiScatteringScaling = (fresnelAverage * fresnelAverage * energyOutgoing) / (1.f - fresnelAverage * (1.f - energyOutgoing));
        multiScatteringLobe *= multiScatteringScaling;
    }
    else if(directMultiscatterBRDF == 2){
        /* 
        simple multiscattering achieved by adding scaled singe scattering lobe, see PBR Filament document
        not using alternative LUT formulation, but this should be equal? Formulation also used by indirect multi scattering paper
        */
        multiScatteringLobe = f0 * (1.f - energyOutgoing) / energyOutgoing * singleScatteringLobe;
    }   
    else {
        multiScatteringLobe = vec3(0.f);
    }
	vec3 specularDirect = directLighting * (singleScatteringLobe + multiScatteringLobe);
    
    //sky occlusion
    float skyOcclusion;
    if(useSkyOcclusion)
    {
        float normalOffset = 0.05f;
        vec3 aoSample = passPos;   
        aoSample -= occlusionVolumeOffset.xyz;
        aoSample += passTBN[2] * normalOffset;  //now in range[-extend/2, extend/2]    
        
        aoSample = aoSample/ occlusionVolumeExtends.xyz;        //in range [-0.5, 0.5]
        aoSample += 0.5f;                                       //in range [0, 1]
        
        vec3 volumeRes = textureSize(sampler3D(skyOcclusionVolume, occlusionSampler), 0);
        
        vec3 motionCorrection = vec3(texelMotionRendering.xyz);
        motionCorrection /= volumeRes;
        aoSample -= motionCorrection;
        
        skyOcclusion = texture(sampler3D(skyOcclusionVolume, occlusionSampler), aoSample).r;
        lightingIndirect *= skyOcclusion;
    }
    
	color = (diffuseDirect + specularDirect) * sunStrengthExposed + lightingIndirect * skyStrengthExposed;
    
    vec3 cascadeTestColor[4] = { 
    vec3(1, 0, 0), 
    vec3(0, 1, 0), 
    vec3(0, 0, 1), 
    vec3(1, 1, 0)};
    //color *= cascadeTestColor[cascadeIndex];

    //color = vec3(skyOcclusion);
}