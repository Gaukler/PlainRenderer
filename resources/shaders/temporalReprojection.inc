//from: "Temporal ReprojectionAnti-Aliasing in INSIDE"
//clips a point towards a target in a axis aligned bounding box
vec3 clipAABB(
    vec3 target,    //last frame color
    vec3 bbMin,     //bounding box min 
    vec3 bbMax      //bounding box max   
    )    
{   
    const vec3 epsilon = vec3(0.0001f);
    
    vec3 center = 0.5f * (bbMax + bbMin);
    vec3 extend = 0.5f * (bbMax - bbMin) + epsilon; //epsilon prevents divison by zero
    
    vec3 toTarget = target - center;
    vec3 toTargetNorm = toTarget / extend;
    vec3 toTargetNormAbs = abs(toTargetNorm);
    float maxComponent = max(toTargetNormAbs.x, max(toTargetNormAbs.y, toTargetNormAbs.z));

    if(maxComponent < 1.f){
        return target; //target in bounding box
    }
    else{
        return center + toTarget / maxComponent;
    }
}

vec3[3][3] sampleNeighbourhood(texture2D inputTexture, sampler inputSampler, vec2 uv, vec2 texelSize){
	vec3[3][3] neighbourhood;
	for(int x = -1; x <= 1; x++){
        for(int y = -1; y <= 1; y++){
			vec3 color = texture(sampler2D(inputTexture, inputSampler), uv + texelSize * vec2(x, y)).rgb;
			neighbourhood[x+1][y+1] = color;
		}
	}
	return neighbourhood;
}

vec3[2] minMaxFromNeighbourhood(vec3[3][3] neighbourhood){
	vec3[2] minMax;
	minMax[0] = neighbourhood[0][0];
	minMax[1] = neighbourhood[0][0];
	for(int i = 0; i < 3; i++){
        for(int j = 0; j < 3; j++){
			minMax[0] = min(minMax[0], neighbourhood[i][j]);
			minMax[1] = max(minMax[1], neighbourhood[i][j]);
		}
	}
	return minMax;
}

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