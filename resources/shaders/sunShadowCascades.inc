#ifndef SUN_SHADOW_CASCADES
#define SUN_SHADOW_CASCADES

const int maxSunShadowCascadeCount = 4;
float shadowSampleRadius = 0.03f; //world space

struct ShadowCascadeInfo{
    vec4 splits;
       mat4 lightMatrices[maxSunShadowCascadeCount];
    vec2 lightSpaceScale[maxSunShadowCascadeCount];
};

float simpleShadow(vec3 posWorld, mat4 lightMatrix, texture2D shadowMap, sampler shadowMapSampler){
    vec4 posLightSpace = lightMatrix * vec4(posWorld, 1.f);
    posLightSpace /= posLightSpace.w;
    posLightSpace.xy = posLightSpace.xy * 0.5f + 0.5f;
    float actualDepth = clamp(posLightSpace.z, 0.f, 1.f);
    float shadowMapDepth = texture(sampler2D(shadowMap, shadowMapSampler), posLightSpace.xy).r;
    return actualDepth > shadowMapDepth ? 1 : 0;
}

vec3 sunShadowCascadeDebugColors(int cascadeIndex){
    vec3 cascadeColors[4] = {
        vec3(1, 0, 0),
        vec3(0, 1, 0),
        vec3(0, 0, 1),
        vec3(1, 1, 0)
    };
    return cascadeColors[cascadeIndex];
}

#endif // #ifndef SUN_SHADOW_CASCADES