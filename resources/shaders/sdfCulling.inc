#ifndef SDF_CULLING_INC
#define SDF_CULLING_INC

uint cullingTileSize = 32;
const uint maxObjectsPerTile = 100;

struct CulledInstancesPerTile{
    uint objectCount;
    uint indices[maxObjectsPerTile];
};

struct BoundingBox{
    vec3 bbMin; float padding1;
    vec3 bbMax; float padding2;
};

uint tileIndexFromTileUV(ivec2 tileUV){
    uint tileCountX = uint(ceil(g_screenResolution.x / float(cullingTileSize.x)));
    return tileUV.x + tileUV.y * tileCountX;
}

#endif // #ifndef SDF_CULLING_INC