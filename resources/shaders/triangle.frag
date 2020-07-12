#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "brdf.inc"

layout(set=0, binding = 0, std140) uniform global{
	vec4 sunColor;
	vec4 sunDirection;
	vec4 ambient;
	mat4 lightMatrix;
	vec4 cameraPosition;
};

layout(set=1, binding = 0) uniform texture2D depthTexture;
layout(set=1, binding = 1) uniform sampler depthSampler;

layout(set=1, binding = 2) 	uniform textureCube	diffuseProbe;
layout(set=1, binding = 3) 	uniform sampler 	cubeSampler;

layout(set=1, binding = 4) 	uniform texture2D 	brdfLutTexture;
layout(set=1, binding = 5) 	uniform textureCube	specularProbe;
layout(set=1, binding = 6) 	uniform sampler 	specularProbeSampler;
layout(set=1, binding = 7) 	uniform sampler 	lutSampler;

layout(set=2, binding = 0) uniform sampler colorSampler;
layout(set=2, binding = 1) uniform sampler normalSampler;
layout(set=2, binding = 2) uniform sampler metalicSampler;
layout(set=2, binding = 3) uniform sampler roughnessSampler;

layout(set=2, binding = 4) uniform texture2D colorTexture;
layout(set=2, binding = 5) uniform texture2D normalTexture;
layout(set=2, binding = 6) uniform texture2D metalicTexture;
layout(set=2, binding = 7) uniform texture2D roughnessTexture;

layout(location = 0) in vec2 passUV;
layout(location = 1) in vec3 passNormal;
layout(location = 2) in vec4 passPos;
layout(location = 3) in mat3 passTBN;

layout(location = 0) out vec3 color;

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

//0: disabled
//1: enabled
layout(constant_id = 2) const int indirectMultiscatterBRDF = 0;

float calcShadow(vec3 pos){
	vec4 posLightSpace = lightMatrix * vec4(pos, 1.f);
	posLightSpace /= posLightSpace.w;
	posLightSpace.xy = posLightSpace.xy * 0.5f + 0.5f;
	float actualDepth = clamp(posLightSpace.z, 0.f, 1.f);
	float shadowMapDepth = texture(sampler2D(depthTexture, depthSampler), posLightSpace.xy).r;
	if(actualDepth <= shadowMapDepth){
		return 1.f;
	}
	return 0.f;
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
	vec3 albedoTexel 		= texture(sampler2D(colorTexture, 		colorSampler), 		passUV).rgb;
	float metalic 			= texture(sampler2D(metalicTexture, 	metalicSampler), 	passUV).r;
	float roughnessTexel 	= texture(sampler2D(roughnessTexture, 	roughnessSampler), 	passUV).r;
	vec3 normalTexel 		= texture(sampler2D(normalTexture, 		normalSampler), 	passUV).rgb;
	
	vec3 albedo = pow(albedoTexel, vec3(2.2f)); //gamma correction
    
	normalTexel = normalTexel * 2.f - 1.f;
	normalTexel.y *= -1.f;	//correct for vulkan coordinate system
	float r = roughnessTexel * roughnessTexel; //remapping
	r = max(r, 0.045f);
	
	vec3 N = normalize(passTBN * normalTexel);
	vec3 L = normalize(sunDirection.xyz);
	vec3 V = normalize(cameraPosition.xyz - passPos.xyz);
	vec3 H = normalize(V + L);
	vec3 R = reflect(-V, N);
	
	const float NoH = max(dot(N, H), 0);
	const float NoL = max(dot(N, L), 0);
	const float NoV = abs(dot(N, V));
	const float VoH = abs(dot(V, H));
    const float LoV = max(dot(L, V), 0.f);
	
	const vec3 f0 = mix(vec3(0.04f), albedo, metalic);
	
    //sun light
	vec3 directLighting = max(dot(N, L), 0.f) * calcShadow(passPos.xyz / passPos.w) * sunColor.rgb;
    
    //direct diffuse
    vec3 diffuseColor = (1.f - metalic) * albedo;
    
    vec3 diffuseDirect;
    
    //lambert
    if(diffuseBRDF == 0){
        diffuseDirect = diffuseColor / 3.1415f * directLighting;
    }
    //disney diffuse
	else if (diffuseBRDF == 1){
        float fresnelDiffuse90 = 0.5f + 2.f * VoH * VoH * r;
        vec3 disneyDiffuse = diffuseColor / 3.1415f * F_Schlick(vec3(1.f), vec3(fresnelDiffuse90), NoL) * F_Schlick(vec3(1.f), vec3(fresnelDiffuse90), NoV);
        
        //energy conservation from frostbite PBR paper
        float energyBias = mix(0.f, 0.5f, r);
        float energyFactor = mix(1.f, 1.f / 1.51f, r);
        float fresnelDiffuse90Biased = energyBias + 2.f * VoH * VoH * r;
        disneyDiffuse = diffuseColor / 3.1415f * F_Schlick(vec3(1.f), vec3(fresnelDiffuse90Biased), NoL) * F_Schlick(vec3(1.f), vec3(fresnelDiffuse90Biased), NoV) * energyFactor;
        diffuseDirect = disneyDiffuse * directLighting;
    }
    //Cod WWII diffuse BRDF, conversion from roughness to gloss computed from papers gloss to roughness formula
    else if (diffuseBRDF == 2){
        float f0Diffuse = VoH + pow(1.f - VoH, 5.f);
        float f1 =  (1.f - 0.75f * pow(1.f - NoL, 5.f)) * 
                    (1.f - 0.75f * pow(1.f - NoV, 5.f));
        float g = log2(2.f / (r * r) - 1.f) / 18.f;
        float t = clamp(2.2f * g - 0.5f, 0.f, 1.f);
        float fd = f0Diffuse + (f1 - f0Diffuse) * t;
        float fb = (34.5f * g * g - 59.f * g + 24.5f) * VoH * pow(2.f, -max(73.2f * g - 21.2f, 8.9f) * sqrt(NoH));
        vec3 fr = diffuseColor / 3.1415f * (fd + fb);
        diffuseDirect = fr * directLighting;              
    }
    //titanfall 2 diffuse from gdc presentation
    else {
        float facing = 0.5f + 0.5f * LoV;
        float rough = facing * (0.9f - 0.4f * facing) * (0.5f + NoH) / max(NoH, 0.03f);
        float smoothDiffuse = 1.05f *   (1.f - pow(1.f - NoL, 5.f)) * 
                                        (1.f - pow(1.f - NoV, 5.f));
        float single = 1.f / 3.1415f * mix(smoothDiffuse, rough, r);
        float multi = 0.1159f * r;
        vec3 diffuseTitanfall2 = diffuseColor * (single + diffuseColor * multi);
        diffuseDirect = diffuseTitanfall2 * directLighting;
    }
	
    //indirect specular
    vec3 lightingIndirect;
    vec2 brdfLut = texture(sampler2D(brdfLutTexture, lutSampler), vec2(r, NoV)).rg;
    vec3 environmentSample = textureLod(samplerCube(specularProbe, specularProbeSampler), R, r * 6.f).rgb;
    vec3 irradiance = texture(samplerCube(diffuseProbe, cubeSampler), N).rgb;
    vec3 fresnelAverage = f0 + (1-f0) / 21.f;
    
    //single scattering only
    if(indirectMultiscatterBRDF == 0){
        vec3 singleScattering = (brdfLut.x * f0 + brdfLut.y);
        lightingIndirect = singleScattering * environmentSample + irradiance * diffuseColor;
    }
    else {
        //multi scattering for IBL from "A Multiple-Scattering Microfacet Model for Real-Time Image-based Lighting"
        vec3 singleScattering = (brdfLut.x * f0 + brdfLut.y); //includes fresnel and energy
        
        float energySingleScattering = brdfLut.x + brdfLut.y;
        float energyMultiScattering = 1 - energySingleScattering;
        vec3 fresnelMultiScattering = singleScattering * fresnelAverage / (1.f - (1.f - energySingleScattering) * fresnelAverage);
        vec3 multiScattering = energyMultiScattering * fresnelMultiScattering;
        
        vec3 energyDiffuseMultiScattering = 1.f - (singleScattering + multiScattering);
        vec3 diffuseCorrection = (1.f - metalic) * albedo * energyDiffuseMultiScattering;
        
        lightingIndirect = singleScattering * environmentSample + (multiScattering + diffuseCorrection) * irradiance;
    }
	
    
    //direct specular
	const float D = D_GGX(NoH, r);
	const float Vis = Visibility(NoV, NoL, r);
	const vec3 F = F_Schlick(f0, vec3(1.f), VoH);
    vec3 singleScatteringLobe = D * Vis * F;
    
    float energyOutgoing = brdfLut.x + brdfLut.y;
    
    /*
    multiscattering formulation from "A Journey Through Implementing Multiscattering BRDFs & Area Lights"
    not working correctly: confusion of "smoothness" parameter of EnergyAverage is it r or 1 - r? if using r too strong for rough materials, using 1-r has effect contrary of what it should look like
    note: smoothness parameter seems to be another kind of gloss parametrization, compare with gloss to roughness from CoD: WWII diffuse
    maybe integrate in octave and fit myself
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
    
    //combine components
	color = diffuseDirect + specularDirect + lightingIndirect;
}