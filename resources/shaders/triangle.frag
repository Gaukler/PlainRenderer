#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "global.inc" 
#include "colorConversion.inc"
#include "brdf.inc"
#include "GeometricAA.inc"
#include "shadowCascadeConstants.inc"
#include "linearDepth.inc"
#include "specularOcclusion.inc"
#include "lightBuffer.inc"
#include "volume.inc"
#include "SphericalHarmonics.inc"

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
layout(constant_id = 3) const bool geometricAA = false;
layout(constant_id = 4) const uint specularProbeMipCount = 0;

//0: traced into textures
//1: sky probes using occlusion volume
layout(constant_id = 5) const int indirectLightingTech = 0;

layout(constant_id = 6) const bool skyProbeUseOcclusion = false;
layout(constant_id = 7) const bool skyProbeUseSkyOcclusionDirection = false;

layout(set=1, binding = 1) 	uniform textureCube	diffuseProbe;

layout(set=1, binding = 3) 	uniform texture2D 	brdfLutTexture;
layout(set=1, binding = 4) 	uniform textureCube	specularProbe;

layout(set=1, binding = 7, std430) buffer lightStorageBuffer{
    LightBuffer lightBuffer;
};

layout(set=1, binding = 8, std430) buffer sunShadowInfo{
    vec4 cascadeSplits;
    mat4 lightMatrices[cascadeCount];
	vec2 lightSpaceScale[4];
};

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
    float weight;
};

layout(set=1, binding = 15) uniform texture2D indirectDiffuse_Y_SH;
layout(set=1, binding = 16) uniform texture2D indirectDiffuse_CoCg;

layout(set=2, binding = 0) uniform texture2D colorTexture;
layout(set=2, binding = 1) uniform texture2D normalTexture;
layout(set=2, binding = 2) uniform texture2D specularTexture;

layout(location = 0) in vec2 passUV;
layout(location = 1) in vec3 passPos;
layout(location = 2) in mat3 passTBN; 

layout(location = 0) out vec3 color;

float shadowTest(texture2D shadowMap, vec2 uv, float actualDepth){
    float depthTexel = texture(sampler2D(shadowMap, g_sampler_nearestBlackBorder), uv, 0).r;
	return float(actualDepth >= depthTexel);
}

//shadow map sampling using spiral sampling pattern
//reference: "The Rendering of Inside", page 43
//reference: "Next Generation Post Processing in Call of Duty Advanced Warfare", page 120
float calcShadow(vec3 pos, float LoV, texture2D shadowMap, mat4 lightMatrix, int cascade){
    
	vec4 posLightSpace = lightMatrix * vec4(pos, 1.f);
	posLightSpace /= posLightSpace.w;
	posLightSpace.xy = posLightSpace.xy * 0.5f + 0.5f;
	float actualDepth = clamp(posLightSpace.z, 0.f, 1.f);
    
	vec2 noiseUV = gl_FragCoord.xy / textureSize(sampler2D(g_noiseTexture, g_sampler_linearRepeat), 0);
	float noise = texture(sampler2D(g_noiseTexture, g_sampler_linearRepeat), noiseUV).r;

	vec2 offsetScale = shadowSampleRadius * lightSpaceScale[cascade];

	float shadow = 0.f;

	float sampleCount = 12.f;
	for(int i = 0; i < sampleCount; i++){
		float d = (i + 0.5f * noise) / sampleCount;
		d = sqrt(d);
		float angle = noise * 2 * pi + 2 * pi * i / sampleCount;

		vec2 offset = vec2(cos(angle), sin(angle));
		offset *= offsetScale * d;

		vec2 samplePosition = posLightSpace.xy + offset;
		shadow += shadowTest(shadowMap, samplePosition, actualDepth);
	}
	return shadow / sampleCount;
}

//mathematical fit from: "A Journey Through Implementing Multiscattering BRDFs & Area Lights"
float EnergyAverage(float roughness){
    float smoothness = 1.f - pow(roughness, 0.25f);
    float r = -0.0761947f - 0.383026f * smoothness;
          r = 1.04997f + smoothness * r;
          r = 0.409255f + smoothness * r;
    return min(0.999f, r);

}

struct SkyOcclusion{
    vec3 unoccludedDirection;
    float factor;
};

SkyOcclusion sampleSkyOcclusion(vec3 worldPos){

	vec3 samplePos = worldPositionToVolume(worldPos, occlusionVolumeOffset.xyz, occlusionVolumeExtends.xyz);    
    vec4 occlusionTexel = texture(sampler3D(skyOcclusionVolume, g_sampler_linearWhiteBorder), samplePos);
    SkyOcclusion occlusion;
    occlusion.unoccludedDirection = normalize(occlusionTexel.rgb);
    occlusion.factor = occlusionTexel.a;
    
    return occlusion;
}

void main(){
	vec3 albedoTexel 		= texture(sampler2D(colorTexture, 		g_sampler_anisotropicRepeat), passUV, g_mipBias).rgb;
	vec3 specularTexel 		= texture(sampler2D(specularTexture, 	g_sampler_anisotropicRepeat), passUV, g_mipBias).rgb;
	vec2 normalTexel 		= texture(sampler2D(normalTexture, 		g_sampler_anisotropicRepeat), passUV, g_mipBias).rg;
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
	const float NoL = clamp(dot(N, L), 0, 1);
	const float VoH = abs(dot(V, H));
    const float LoV = max(dot(L, V), 0.f);
    
    float NoV = abs(dot(N, V));
    //0 is physically impossible and breaks several math formulas
    //can occur because of normal mapping, must be avoided
	NoV = max(NoV, 0.0001f);
    
	const vec3 f0 = mix(vec3(0.04f), albedo, metalic);
    
    //sun light
    float sunShadow;
    int cascadeIndex = 0;
    for(int cascade = 0; cascade < cascadeCount - 1; cascade++){
        cascadeIndex += int(pixelDistance >= cascadeSplits[cascade]);
    }
    if(cascadeIndex == 0){
        sunShadow = calcShadow(passPos, LoV, shadowMapCascade0, lightMatrices[cascadeIndex], 0);
    }
    else if(cascadeIndex == 1){
        sunShadow = calcShadow(passPos, LoV, shadowMapCascade1, lightMatrices[cascadeIndex], 1);
    }
    else if(cascadeIndex == 2){
        sunShadow = calcShadow(passPos, LoV, shadowMapCascade2, lightMatrices[cascadeIndex], 2);
    }
    else if(cascadeIndex == 3){
        sunShadow = calcShadow(passPos, LoV, shadowMapCascade3, lightMatrices[cascadeIndex], 3);
    }
	vec3 directLighting = max(dot(N, L), 0.f) * sunShadow * lightBuffer.sunColor;
	
    //sky occlusion
    SkyOcclusion skyOcclusion;
    {
        vec3 geoN = normalize(passTBN[2]);
        float normalOffset = 0.5f;
        
        vec3 aoSamplePoint = passPos;
        aoSamplePoint += normalOffset * geoN;   
        
        skyOcclusion = sampleSkyOcclusion(aoSamplePoint);
    }
    
    vec3 fresnelAverage = f0 + (1-f0) / 21.f;

    vec3 irradiance;
	vec3 environmentSample;
	float r_indirect = r;	//roughness for indirect lighting might be modified to approximate look of more ambient light

	//indirect lighting is traced into texture
	if(indirectLightingTech == 0){
		//diffuse
		vec2 screenUV = gl_FragCoord.xy / g_screenResolution;
		vec4 irradiance_Y_SH = texture(sampler2D(indirectDiffuse_Y_SH, g_sampler_nearestClamp), screenUV);
		float irradiance_Y = dot(irradiance_Y_SH, directionToSH_L1(N));
		vec2 irradiance_CoCg = texture(sampler2D(indirectDiffuse_CoCg, g_sampler_nearestClamp), screenUV).rg;
		irradiance = YCoCgToLinear(vec3(irradiance_Y, irradiance_CoCg));

		//specular
		float SHDirectionLength = length(irradiance_Y_SH.wyz);
		vec3 SHDirection = irradiance_Y_SH.wyz / SHDirectionLength;
		SHDirectionLength = clamp(SHDirectionLength, 0.01, 1);
		r_indirect = mix(r, 1, sqrt(SHDirectionLength));

		environmentSample = YCoCgToLinear(vec3(max(dot(directionToSH_L1(R), irradiance_Y_SH), 0), irradiance_CoCg));
	}
	else if(indirectLightingTech == 1){
		//diffuse
		if(skyProbeUseSkyOcclusionDirection){
			irradiance = texture(samplerCube(diffuseProbe, g_sampler_linearRepeat), skyOcclusion.unoccludedDirection).rgb;
		}
		else{
			irradiance = texture(samplerCube(diffuseProbe, g_sampler_linearRepeat), N).rgb;
		}

		//specular
		//indirect specular
		float probeLoD = specularProbeMipCount * r;
		environmentSample = textureLod(samplerCube(specularProbe, g_sampler_linearClamp), R, probeLoD).rgb;

		if(skyProbeUseOcclusion){
			irradiance *= skyOcclusion.factor;
        
			float specularOcclusion = computeSpecularOcclusion(R, skyOcclusion.unoccludedDirection, r, skyOcclusion.factor);
			environmentSample *= specularOcclusion;
		}
	}

	vec3 brdfLut = texture(sampler2D(brdfLutTexture, g_sampler_linearClamp), vec2(r_indirect, NoV)).rgb;

	//direct diffuse    
	vec3 diffuseColor = (1.f - metalic) * albedo;
    vec3 diffuseDirect;
    vec3 diffuseBRDFIntegral = vec3(1.f);
    
    //lambert
    if(diffuseBRDF == 0){
        diffuseDirect = diffuseColor / pi * directLighting;
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
        float multiIntegral = 0.1159f * r * pi * 2.f;
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
    diffuseDirect *= (1.f - F_Schlick(f0, vec3(1.f), NoV)) * (1.f - (F_Schlick(f0, vec3(1.f), NoL)));

    vec3 diffuseIndirect;
    vec3 specularIndirect;
    if(indirectMultiscatterBRDF){
        //multi scattering for IBL from "A Multiple-Scattering Microfacet Model for Real-Time Image-based Lighting"
        vec3 singleScattering = (brdfLut.x * f0 + brdfLut.y); //includes fresnel and energy
        
        float energySingleScattering = brdfLut.x + brdfLut.y;
        float energyMultiScattering = 1 - energySingleScattering;
        vec3 fresnelMultiScattering = singleScattering * fresnelAverage / (1.f - (1.f - energySingleScattering) * fresnelAverage);
        vec3 multiScattering = energyMultiScattering * fresnelMultiScattering;
        
        vec3 energyDiffuseMultiScattering = 1.f - (singleScattering + multiScattering);
        vec3 diffuseCorrection = diffuseColor * energyDiffuseMultiScattering;
        
        diffuseIndirect = diffuseCorrection * diffuseBRDFIntegral * irradiance;
        specularIndirect = singleScattering * environmentSample + multiScattering * irradiance;
    }
    else {
        //single scattering only
        vec3 singleScattering = (brdfLut.x * f0 + brdfLut.y);
        diffuseIndirect = irradiance * diffuseColor * diffuseBRDFIntegral;
        specularIndirect = singleScattering * environmentSample;
    }

    vec3 lightingIndirect = diffuseIndirect + specularIndirect;

    //direct specular
	const float D = D_GGX(NoH, r);
	const float Vis = Visibility(NoV, NoL, r);
	const vec3 F = F_Schlick(f0, vec3(1.f), VoH);
    vec3 singleScatteringLobe = D * Vis * F;
    
    float energyOutgoing = brdfLut.x + brdfLut.y;
    
    //multiscattering formulation from "A Journey Through Implementing Multiscattering BRDFs & Area Lights"
    vec3 multiScatteringLobe;
    if(directMultiscatterBRDF == 0){
        float energyAverage = EnergyAverage(r);
    
        vec2 brdfLutIncoming = texture(sampler2D(brdfLutTexture, g_sampler_linearClamp), vec2(r, NoL)).rg;
        float energyIncoming = brdfLutIncoming.x + brdfLutIncoming.y;
        
        float multiScatteringLobeFloat = (1.f - energyIncoming) * (1.f - energyOutgoing) / (3.1415f * (1.f - energyAverage));
        vec3 multiScatteringScaling = (fresnelAverage * fresnelAverage * energyAverage) / (1.f - fresnelAverage * (1.f - energyAverage));
        
        multiScatteringLobe = multiScatteringLobeFloat * multiScatteringScaling;
    }
    //this is the above but approximating E_avg = E_o, simplifying the equation
    else if(directMultiscatterBRDF == 1){
        multiScatteringLobe = vec3((1.f - energyOutgoing) / pi);
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

    color = (diffuseDirect + specularDirect) * lightBuffer.sunStrengthExposed + lightingIndirect;
	color = (diffuseDirect + specularDirect) * lightBuffer.sunStrengthExposed + lightingIndirect;
	//color = irradiance / pi;
}