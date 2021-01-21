#include "volume.inc"

struct VolumeInfo{
	vec4 extends;
    vec4 offset;
};

float sampleSDF(vec3 uv, texture3D sdfTexture){
	return texture(sampler3D(sdfTexture, g_sampler_linearClamp), uv).r;
}

vec3 normalFromSDF(vec3 uv, VolumeInfo sdfInfo, texture3D sdfTexture){
	float extendsMax = max(sdfInfo.extends.x, max(sdfInfo.extends.y, sdfInfo.extends.z));
	vec3 extendsNormalized = sdfInfo.extends.xyz / extendsMax;
	vec3 epsilon = vec3(0.05f) / textureSize(sampler3D(sdfTexture, g_sampler_linearClamp), 0) / extendsNormalized;	//voxels are anisotropic so epsilon must be scaled per axis
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

//binary shadowing function
//aabb min and max are min and max of sdf aabb, used to determine if shadow ray left scene
//returns factor to be multiplied with light
//soft shadows would require a higher resolution sdf
//might revisit if sdf quality is improved
float shadowRay(vec3 startPosWorld, float sdfThreshold, vec3 aabbMin, vec3 aabbMax, VolumeInfo sdfInfo, texture3D sdfTexture){
	int sampleCount = 80;

	vec3 samplePosWorld = startPosWorld;

	for(int i = 0; i < sampleCount; i++){
		vec3 samplePos = worldPositionToVolume(samplePosWorld, sdfInfo.offset.xyz, sdfInfo.extends.xyz);

		float d = sampleSDF(samplePos, sdfTexture);
		bool hit = d < sdfThreshold;

		if(hit){
			return 0.f;
		}

		float stepSize = d;
		samplePosWorld += g_sunDirection.xyz * stepSize;

		if(!isPointInAABB(samplePosWorld, aabbMin, aabbMax)){
			1.f;
		}
	}
	//not enough samples, assume lit
	return 1.f;	
}

struct HitResult{
	bool hit;
	float t;	//hit = origin + direction * t
};

HitResult rayAABBIntersection(vec3 rayOrigin, vec3 rayDirection, vec3 aabbMin, vec3 aabbMax, VolumeInfo volumeInfo){

	HitResult result;
	result.hit = false;
	result.t = 100000;	//will be replaced by min

	//search for bounding box intersection
	float intersection =  rayOrigin.x < volumeInfo.offset.x ? aabbMin.x : aabbMax.x;
	
	float tx = (intersection - rayOrigin.x) / rayDirection.x;
	vec3 planeIntersection = rayOrigin + tx * rayDirection;
	if(	tx > 0 && 
		planeIntersection.y >= aabbMin.y && 
		planeIntersection.y <= aabbMax.y &&
		planeIntersection.z >= aabbMin.z && 
		planeIntersection.z <= aabbMax.z){
		result.t = min(result.t, tx);
		result.hit = true;
	}

	intersection =  rayOrigin.y < volumeInfo.offset.y ? aabbMin.y : aabbMax.y;
	float ty = (intersection - rayOrigin.y) / rayDirection.y;
	planeIntersection = rayOrigin + ty * rayDirection;
	if(	ty > 0 && 
		planeIntersection.x >= aabbMin.x && 
		planeIntersection.x <= aabbMax.x &&
		planeIntersection.z >= aabbMin.z && 
		planeIntersection.z <= aabbMax.z){
		result.t = min(result.t, ty);
		result.hit = true;
	}

	intersection =  rayOrigin.z < volumeInfo.offset.z ? aabbMin.z : aabbMax.z;
	float tz = (intersection - rayOrigin.z) / rayDirection.z;
	planeIntersection = rayOrigin + tz * rayDirection;
	if(	tz > 0 && 
		planeIntersection.x >= aabbMin.x && 
		planeIntersection.x <= aabbMax.x &&
		planeIntersection.y >= aabbMin.y && 
		planeIntersection.y <= aabbMax.y){
		result.t = min(result.t, tz);
		result.hit = true;
	}
	return result;
}

struct SDFTrace{
	bool hit;
	vec3 hitPosOriginal;
	vec3 hitPosCalculated;
};

SDFTrace sdfTrace(vec3 rayOrigin, vec3 rayDirection, VolumeInfo sdfInfo, texture3D sdfTexture, float sdfThreshold, vec3 sdfMin, vec3 sdfMax){

	SDFTrace result;
	result.hit = false;
	float d = 0.f;		//distance to surface
	float dLast = 0.f;	//last step distance

	int sampleCount = 128;

	vec3 samplePosWorld = rayOrigin;

	for(int i = 0; i < sampleCount; i++){
		vec3 samplePos = worldPositionToVolume(samplePosWorld, sdfInfo.offset.xyz, sdfInfo.extends.xyz);

		if(!isPointInAABB(samplePos, sdfMin, sdfMax)){
			result.hit = false;
			return result;
		}

		dLast = d;
		d = sampleSDF(samplePos, sdfTexture);

		result.hit = d < sdfThreshold;
		
		if(result.hit){
			break;
		}
		samplePosWorld += rayDirection * abs(d);
	}

	result.hitPosOriginal = samplePosWorld;

	float lastStepSize = d / (1.f - (d - dLast));

	//assume surface is planar and calculate hit using geometric series
	//see: "GPU-based clay simulation and ray-tracing tech in Claybook", page 25
	result.hitPosCalculated = result.hitPosOriginal + rayDirection * lastStepSize;

	return result;
}

vec3 shadeHit(SDFTrace trace, VolumeInfo sdfInfo, texture3D sdfTexture, texture3D materialTexture, float sdfThreshold, 
	vec3 ambient, vec3 sdfMin, vec3 sdfMax, vec3 sunColor, float sunStrengthExposed){

	//normal calculation is based on original position
	//the modified position is more accurate but on thin objects the modification might push the position to the other side
	//because of this normals are more consistent when using the original position
	vec3 hitOriginalNormalized = worldPositionToVolume(trace.hitPosOriginal, sdfInfo.offset.xyz, sdfInfo.extends.xyz);
	vec3 N = normalFromSDF(hitOriginalNormalized, sdfInfo, sdfTexture);

	//material lookup uses the modified position as it is more accurate
	vec3 hitModifiedNormalized = worldPositionToVolume(trace.hitPosCalculated, sdfInfo.offset.xyz, sdfInfo.extends.xyz);
	vec4 albedoTexel = texture(sampler3D(materialTexture, g_sampler_linearClamp), hitModifiedNormalized);
	vec3 albedo = albedoTexel.rgb / max(albedoTexel.a, 0.00001);

	//shadow ray origin is offset into normal direction to avoid self intersection
	vec3 shadowRayOrigin = trace.hitPosOriginal + N * sdfThreshold;
	float shadow = shadowRay(shadowRayOrigin, sdfThreshold, sdfMin, sdfMax, sdfInfo, sdfTexture);

	vec3 sunLight = shadow * dot(N, g_sunDirection.xyz) * sunStrengthExposed * sunColor;
	return albedo * (sunLight + ambient);
}