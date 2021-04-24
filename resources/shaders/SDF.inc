#ifndef SDF_INC
#define SDF_INC

struct SDFInstance{
    vec3    localExtends;
    uint    sdfTextureIndex;
    vec3    meanAlbedo;
    float   padding;
    mat4x4  worldToLocal;
};

float sampleSDF(vec3 uv, texture3D sdfTexture){
    return texture(sampler3D(sdfTexture, g_sampler_linearClamp), uv).r;
}

vec3 normalFromSDF(vec3 uv, vec3 extends, texture3D sdfTexture){
    float extendsMax = max(extends.x, max(extends.y, extends.z));
    vec3 extendsNormalized = extends.xyz / extendsMax;
    vec3 epsilon = vec3(0.15f) / textureSize(sampler3D(sdfTexture, g_sampler_linearClamp), 0) / extendsNormalized;	//voxels are anisotropic so epsilon must be scaled per axis
    return normalize(vec3(
        sampleSDF(uv + vec3(epsilon.x, 0, 0), sdfTexture) - sampleSDF(uv - vec3(epsilon.x, 0, 0), sdfTexture),
        sampleSDF(uv + vec3(0, epsilon.y, 0), sdfTexture) - sampleSDF(uv - vec3(0, epsilon.y, 0), sdfTexture),
        sampleSDF(uv + vec3(0, 0, epsilon.z), sdfTexture) - sampleSDF(uv - vec3(0, 0, epsilon.z), sdfTexture)
    ));
}

bool isPointInAABB(vec3 p, vec3 aabbMin, vec3 aabbMax){
    return 
        p.x >= aabbMin.x &&
        p.y >= aabbMin.y &&
        p.z >= aabbMin.z &&
        p.x <= aabbMax.x &&
        p.y <= aabbMax.y &&
        p.z <= aabbMax.z;
}

struct HitResult{
    bool hit;
    float t;	//hit = origin + direction * t
};

HitResult rayAABBIntersection(vec3 rayOrigin, vec3 rayDirection, vec3 aabbMin, vec3 aabbMax){

    HitResult result;
    result.hit = false;
    result.t = 100000;	//will be replaced by min

    //search for bounding box intersection
    float intersection =  rayOrigin.x < 0 ? aabbMin.x : aabbMax.x;

    float tx = (intersection - rayOrigin.x) / rayDirection.x;
    vec3 planeIntersection = rayOrigin + tx * rayDirection;
    if( tx > 0 &&
        planeIntersection.y >= aabbMin.y &&
        planeIntersection.y <= aabbMax.y &&
        planeIntersection.z >= aabbMin.z &&
        planeIntersection.z <= aabbMax.z){
        result.t = min(result.t, tx);
        result.hit = true;
    }

    intersection =  rayOrigin.y < 0 ? aabbMin.y : aabbMax.y;
    float ty = (intersection - rayOrigin.y) / rayDirection.y;
    planeIntersection = rayOrigin + ty * rayDirection;
    if( ty > 0 && 
        planeIntersection.x >= aabbMin.x &&
        planeIntersection.x <= aabbMax.x &&
        planeIntersection.z >= aabbMin.z &&
        planeIntersection.z <= aabbMax.z){
        result.t = min(result.t, ty);
        result.hit = true;
    }

    intersection =  rayOrigin.z < 0 ? aabbMin.z : aabbMax.z;
    float tz = (intersection - rayOrigin.z) / rayDirection.z;
    planeIntersection = rayOrigin + tz * rayDirection;
    if( tz > 0 &&
        planeIntersection.x >= aabbMin.x &&
        planeIntersection.x <= aabbMax.x &&
        planeIntersection.y >= aabbMin.y &&
        planeIntersection.y <= aabbMax.y){
        result.t = min(result.t, tz);
        result.hit = true;
    }
    return result;
}

struct TraceResult{
    bool hit;
    float closestHitDistance;
    vec3 hitPos; 
    vec3 N; 
    int hitCount;
    vec3 albedo;
    bool selfIntersection;
};

vec3 localSamplePositionToUV(vec3 localPosition, vec3 AABBExtends){
    return localPosition / AABBExtends + 0.5;
}

void traceRayTroughSDFInstance(SDFInstance instance, vec3 rayStartWorld, texture3D sdfTexture, vec3 rayDirectionWorld, inout TraceResult traceResult){

    vec3 rayStartLocal	= (instance.worldToLocal * vec4(rayStartWorld, 1)).xyz;
    vec3 rayEndLocal	= (instance.worldToLocal * vec4(rayStartWorld + rayDirectionWorld, 1)).xyz;

    vec3 rayDirection = rayEndLocal - rayStartLocal;
    rayDirection /= length(rayDirection);

    vec3 sdfMaxLocal = instance.localExtends * 0.5;
    vec3 sdfMinLocal = -sdfMaxLocal;

    float hitDistanceLocal = 0.f;

    if(!isPointInAABB(rayStartLocal, sdfMinLocal, sdfMaxLocal)){
        HitResult aabbHit = rayAABBIntersection(rayStartLocal, rayDirection, sdfMinLocal, sdfMaxLocal);

        if(aabbHit.hit){
            //move sample point to intersection
            rayStartLocal += (aabbHit.t) * rayDirection;
            hitDistanceLocal = aabbHit.t;
        }
        else{
            //ray missed AABB
            return;
        }
    }

    vec3 localSamplePos = rayStartLocal;

    float distanceThreshold = 0.1f;
    ivec3 sdfResolution = textureSize(sampler3D(sdfTexture, g_sampler_linearClamp), 0);
    distanceThreshold = length(instance.localExtends / sdfResolution) * 0.25;

    float dLast = 0.f;	//last step distance
    float d = 0.f;

    float localToGlobalScale = 1.f / length(instance.worldToLocal[0].xyz);

    //skip object if AABB intersection is further away than current closest hit
    if(localToGlobalScale * hitDistanceLocal > traceResult.closestHitDistance){
        return;
    }

    for(int i = 0; i < 128; i++){
    
        vec3 localExtendsHalf = instance.localExtends * 0.5;
        localExtendsHalf += 0.01; //bias
        if(any(greaterThan(localSamplePos, localExtendsHalf)) || any(lessThan(localSamplePos, -localExtendsHalf))){
            break;
        }

        vec3 sampleUV = localSamplePositionToUV(localSamplePos, instance.localExtends);

        dLast = d;
        d = texture(sampler3D(sdfTexture, g_sampler_linearClamp), sampleUV).r; 

        if(d < distanceThreshold){
            traceResult.hit = true;

            float distanceGlobal = hitDistanceLocal * localToGlobalScale;

            if(distanceGlobal < traceResult.closestHitDistance){
                traceResult.closestHitDistance = distanceGlobal;
                traceResult.hitCount = i;

                float lastStepSizeLocal = d / (1.f - (d - dLast));

                //assume surface is planar and calculate hit using geometric series
                //see: "GPU-based clay simulation and ray-tracing tech in Claybook", page 25
                localSamplePos += rayDirection * lastStepSizeLocal;
                sampleUV = localSamplePos / instance.localExtends + 0.5;

                traceResult.N = normalFromSDF(sampleUV, instance.localExtends, sdfTexture);
                traceResult.N = transpose(mat3(instance.worldToLocal)) * traceResult.N;	//worldToLocal is rotation matrix, so transpose is inverse
                traceResult.albedo = pow(instance.meanAlbedo, vec3(2.2f));
                float lastStepSizeGlobal = lastStepSizeLocal * localToGlobalScale;
                traceResult.hitPos = rayStartWorld + rayDirectionWorld * (distanceGlobal + lastStepSizeGlobal);
            }
            break;
        }
        localSamplePos += rayDirection * abs(d);
        hitDistanceLocal += abs(d);
    }
}

#endif // #ifndef SDF_INC