uint cullingTileSize = 32;
const uint maxObjectsPerTile = 100;
uint shadowTileCount1D = 32;

struct CulledInstancesPerTile{
	uint objectCount;
	uint indices[maxObjectsPerTile];
};

struct ShadowFrustum{
	vec3 up;		float padding0;
	vec3 right;		float padding1;
	vec2 extends;
	vec2 offset;
};

struct BoundingBox{
		vec3 bbMin; float padding1;
		vec3 bbMax; float padding2;
};

uint tileIndexFromTileUV(ivec2 tileUV){
	uint tileCountX = uint(ceil(g_screenResolution.x / float(cullingTileSize.x)));
	return tileUV.x + tileUV.y * tileCountX;
}

uint computeShadowTileIndex(vec3 positionWorld, ShadowFrustum shadowFrustum){
	vec2 shadowFrustumUV = vec2(dot(positionWorld, shadowFrustum.right), dot(positionWorld, shadowFrustum.up));
	shadowFrustumUV -= shadowFrustum.offset;
	shadowFrustumUV /= shadowFrustum.extends;
	ivec2 shadowTileUV = ivec2(shadowFrustumUV * shadowTileCount1D);
	return shadowTileUV.x + shadowTileUV.y * shadowTileCount1D;
}

bool isShadowTileValid(ivec2 shadowTileUV){
	return  all(greaterThanEqual(shadowTileUV, ivec2(0))) && all(lessThan(shadowTileUV, ivec2(shadowTileCount1D)));
}