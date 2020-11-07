vec2 getClosestFragmentMotion(ivec2 uv, texture2D depthTexture, texture2D velocityTexture, sampler textureSampler){
    float closestDepth = 1;
    ivec2 closestDepthOffset = ivec2(0);
    
    //3x3 neighbourhood from Inside paper
    for(int x = -1; x <= 1; x++){
        for(int y = -1; y <= 1; y++){
            ivec2 offset = ivec2(x, y);
            float depth = texelFetch(sampler2D(depthTexture, textureSampler), uv + offset, 0).r;
            if(depth < closestDepth){
                closestDepth = depth;
                closestDepthOffset = offset;
            }
        }
    }
    return texelFetch(sampler2D(velocityTexture, textureSampler), uv + closestDepthOffset, 0).rg;
}