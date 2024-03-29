#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "global.inc" 
#include "colorConversion.inc"
#include "brdf.inc"
#include "GeometricAA.inc"
#include "sunShadowCascades.inc"
#include "linearDepth.inc"
#include "lightBuffer.inc"
#include "SphericalHarmonics.inc"
#include "volumetricFroxelLighting.inc"

//---- specialisation constants ----

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
layout(constant_id = 2) const bool geometricAA = false;

//0: traced into textures
//1: constant ambient
layout(constant_id = 3) const int indirectLightingTech = 0;
layout(constant_id = 4) const uint sunShadowCascadeCount = 4;

//---- resource bindings ----

layout(set=1, binding = 1) 	uniform textureCube	diffuseProbe;

layout(set=1, binding = 3) 	uniform texture2D 	brdfLutTexture;
layout(set=1, binding = 4) 	uniform textureCube	specularProbe;

layout(set=1, binding = 7, std430) buffer lightStorageBuffer{
    LightBuffer lightBuffer;
};

layout(set=1, binding = 8, std430) buffer sunShadowInfo{
    ShadowCascadeInfo sunShadowCascadeInfo;
};

layout(set=1, binding = 9)  uniform texture2D shadowMapCascade0;
layout(set=1, binding = 10) uniform texture2D shadowMapCascade1;
layout(set=1, binding = 11) uniform texture2D shadowMapCascade2;
layout(set=1, binding = 12) uniform texture2D shadowMapCascade3;

layout(set=1, binding = 15) uniform texture2D indirectDiffuse_Y_SH;
layout(set=1, binding = 16) uniform texture2D indirectDiffuse_CoCg;

layout(set=1, binding = 18) uniform texture3D volumetricLightingLUT;

layout(set=1, binding = 19) uniform SettingsBuffer {
    VolumetricLightingSettings volumetricSettings;
};

layout(push_constant) uniform MatrixBlock {
    int albedoTextureIndex;
    int normalTextureIndex;
    int specularTextureIndex;
    int transformIndex;
};

layout(set=2, binding = 0) uniform texture2D[] textures;

//---- in/out variables ----

layout(location = 0) in vec2 passUV;
layout(location = 1) in vec3 passPos;
layout(location = 2) in mat3 passTBN; 

layout(location = 0) out vec3 color;

//---- functions ----

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

    uint noiseIndex = g_frameIndexMod4;
    vec2 noiseUV = vec2(gl_FragCoord.xy) / textureSize(sampler2D(textures[g_noiseTextureIndices[noiseIndex]], g_sampler_linearRepeat), 0);
    float noise = texture(sampler2D(textures[g_noiseTextureIndices[noiseIndex]], g_sampler_nearestRepeat), noiseUV).r;

    vec2 offsetScale = shadowSampleRadius * sunShadowCascadeInfo.lightSpaceScale[cascade];

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
float ReflectedEnergyAverage(float roughness){
    //function is fitted to smoothness, must calculate from roughness
    //r = sqrt(1 - smoothness)
    float smoothness = 1.f - sqrt(roughness);
    float r = -0.0761947f - 0.383026f * smoothness;
          r = 1.04997f + smoothness * r;
          r = 0.409255f + smoothness * r;
    return min(0.999f, r);
}

vec3 applyVolumetricLighting(float pixelDepth, vec3 V){
    uint noiseIndex = g_frameIndexMod4;
    vec2 noiseUV = vec2(gl_FragCoord.xy) / textureSize(sampler2D(textures[g_noiseTextureIndices[noiseIndex]], g_sampler_linearRepeat), 0);
    vec2 noise = texture(sampler2D(textures[g_noiseTextureIndices[noiseIndex]], g_sampler_nearestRepeat), noiseUV).rg;
    noise -= 0.5;
    noise *= 0.013;

    vec2 screenUV = gl_FragCoord.xy / g_screenResolution.xy;
    screenUV += noise;
    vec4 inscatteringTransmittance = volumeTextureLookup(screenUV, pixelDepth, volumetricLightingLUT, volumetricSettings.maxDistance);
    return applyInscatteringTransmittance(color, inscatteringTransmittance);
}

vec3 computeSpecularMultiscatteringLobe(float r, float NoL, vec3 f0, vec3 singleScatteringLobe, vec3 brdfLut){
    vec3 multiScatteringLobe;
    float energyOutgoing = brdfLut.y;
    vec3 fresnelAverage = f0 + (1.f - f0) / 21.f;

    //multiscattering formulation from "A Journey Through Implementing Multiscattering BRDFs & Area Lights"
    if(directMultiscatterBRDF == 0){
        float energyAverage = ReflectedEnergyAverage(r);
        float energyIncoming = texture(sampler2D(brdfLutTexture, g_sampler_linearClamp), vec2(r, NoL)).y;

        float multiScatteringLobeUnscaled = (1.f - energyIncoming) * (1.f - energyOutgoing) / (3.1415f * (1.f - energyAverage));
        vec3 multiScatteringScaling = (fresnelAverage * fresnelAverage * energyAverage) / (1.f - fresnelAverage * (1.f - energyAverage));

        multiScatteringLobe = multiScatteringLobeUnscaled * multiScatteringScaling;
    }
    //this is the above but approximating E_avg = E_o, simplifying the equation
    else if(directMultiscatterBRDF == 1){
        multiScatteringLobe = vec3((1.f - energyOutgoing) / pi);
        vec3 multiScatteringScaling = (fresnelAverage * fresnelAverage * energyOutgoing) / (1.f - fresnelAverage * (1.f - energyOutgoing));
        multiScatteringLobe *= multiScatteringScaling;
    }
    else if(directMultiscatterBRDF == 2){ 
        //simple multiscattering achieved by adding scaled singe scattering lobe, see PBR Filament document
        multiScatteringLobe = f0 * (1.f / energyOutgoing - 1.f) * singleScatteringLobe;
    }
    else {
        multiScatteringLobe = vec3(0.f);
    }
    return multiScatteringLobe;
}

void main(){
    vec3 albedoTexel 		= texture(sampler2D(textures[albedoTextureIndex], g_sampler_anisotropicRepeat), passUV, g_mipBias).rgb;
    vec3 specularTexel 		= texture(sampler2D(textures[specularTextureIndex], g_sampler_anisotropicRepeat), passUV, g_mipBias).rgb;
    vec2 normalTexel 		= texture(sampler2D(textures[normalTextureIndex], g_sampler_anisotropicRepeat), passUV, g_mipBias).rg;
    vec3 normalTexelReconstructed = vec3(normalTexel, sqrt(1.f - normalTexel.x * normalTexel.x + normalTexel.y + normalTexel.y));
    normalTexelReconstructed = normalTexelReconstructed * 2.f - 1.f;

    float microAO = specularTexel.r; //not used
    float metalic = specularTexel.b;
    float r = specularTexel.g;

    r = max(r * r, 0.0045f);
    vec3 albedo = sRGBToLinear(albedoTexel);

    vec3 diffuseColor = (1.f - metalic) * albedo;

    vec3 N = normalize(passTBN * normalTexelReconstructed);
    if(any(isnan(N))){
        N = passTBN[2]; //fix for broken (bi)tangents on tree assets
    }
    vec3 L = normalize(g_sunDirection.xyz);

    vec3 V = g_cameraPosition.xyz - passPos;
    float pixelDepth = dot(V, -g_cameraForward.xyz); 
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
    for(int cascade = 0; cascade < sunShadowCascadeCount - 1; cascade++){
        cascadeIndex += int(pixelDepth >= sunShadowCascadeInfo.splits[cascade]);
    }
    if(cascadeIndex == 0){
        sunShadow = calcShadow(passPos, LoV, shadowMapCascade0, sunShadowCascadeInfo.lightMatrices[cascadeIndex], 0);
    }
    else if(cascadeIndex == 1){
        sunShadow = calcShadow(passPos, LoV, shadowMapCascade1, sunShadowCascadeInfo.lightMatrices[cascadeIndex], 1);
    }
    else if(cascadeIndex == 2){
        sunShadow = calcShadow(passPos, LoV, shadowMapCascade2, sunShadowCascadeInfo.lightMatrices[cascadeIndex], 2);
    }
    else if(cascadeIndex == 3){
        sunShadow = calcShadow(passPos, LoV, shadowMapCascade3, sunShadowCascadeInfo.lightMatrices[cascadeIndex], 3);
    }
    vec3 directLighting = max(dot(N, L), 0.f) * sunShadow * lightBuffer.sunColor;
    vec3 brdfLut = texture(sampler2D(brdfLutTexture, g_sampler_linearClamp), vec2(r, NoV)).rgb;

    //direct diffuse
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

    //direct specular
    vec3 singleScatteringLobe = GGXSingleScattering(r, f0, NoH, NoV, VoH, NoL);
    vec3 multiScatteringLobe = computeSpecularMultiscatteringLobe(r, NoL, f0, singleScatteringLobe, brdfLut);
    vec3 specularDirect = directLighting * (singleScatteringLobe + multiScatteringLobe);

    vec3 lightingIndirect;

    //indirect lighting is traced into texture
    if(indirectLightingTech == 0){
        //diffuse
        vec2 screenUV = gl_FragCoord.xy / g_screenResolution;
        vec4 irradiance_Y_SH = texture(sampler2D(indirectDiffuse_Y_SH, g_sampler_nearestClamp), screenUV);
        float irradiance_Y = dot(irradiance_Y_SH, directionToSH_L1(N));
        vec2 irradiance_CoCg = texture(sampler2D(indirectDiffuse_CoCg, g_sampler_nearestClamp), screenUV).rg;
        vec3 irradiance = YCoCgToLinear(vec3(irradiance_Y, irradiance_CoCg));

        vec3 diffuseIndirect = irradiance * diffuseColor * diffuseBRDFIntegral;

        //specular
        vec3 dominantDirection = dominantDirectionFromSH_L1(irradiance_Y_SH);
        float dominantDirectionLength = length(dominantDirection);
        dominantDirectionLength = clamp(dominantDirectionLength, 0.01, 1);
        float r_indirect = mix(1, r, sqrt(dominantDirectionLength));

        vec3 L_indirect = dominantDirection / dominantDirectionLength;
        vec3 H_indirect = normalize(L_indirect+V);
        float NoH_indirect = max(dot(N, H_indirect), 0.f);
        float NoL_indirect = max(dot(N, L_indirect), 0.f);
        float VoH_indirect = max(dot(V, H_indirect), 0.f);

        vec3 singleScattering_indirect = GGXSingleScattering(r_indirect, f0, NoH_indirect, NoV, VoH_indirect, NoL_indirect);
        vec3 multiScattering_indirect = computeSpecularMultiscatteringLobe(r_indirect, NoL_indirect, f0, singleScattering_indirect, brdfLut);

        vec3 specularIndirect = (singleScattering_indirect + multiScattering_indirect) * YCoCgToLinear(vec3(irradiance_Y_SH.x, irradiance_CoCg)); 
        lightingIndirect = diffuseIndirect + specularIndirect;
    }
    //constant ambient
    else {
        float ambientStrength = 0.003f;
        vec3 irradiance	= vec3(ambientStrength) * lightBuffer.sunStrengthExposed;
        vec3 reflection	= vec3(ambientStrength) * lightBuffer.sunStrengthExposed;

        vec3 singleScattering = mix(vec3(brdfLut.x), vec3(brdfLut.y), f0);
        vec3 diffuseIndirect = irradiance * diffuseColor * diffuseBRDFIntegral;
        vec3 specularIndirect = singleScattering * reflection;
        lightingIndirect = diffuseIndirect + specularIndirect;
    }

    color = (diffuseDirect + specularDirect) * lightBuffer.sunStrengthExposed + lightingIndirect;
    color = applyVolumetricLighting(pixelDepth, V);

    //color = N*0.5+0.5;
    //color = passPos;
    //color = sunShadowCascadeDebugColors(cascadeIndex);
}