//returns normalized volume coordinates
//if input is within volume result is in range [0:1]
vec3 worldPositionToVolume(vec3 pWorld, vec3 volumeOffset, vec3 volumeExtent){
	vec3 samplePos = pWorld - volumeOffset;	//in range[-extend/2, extend/2]
    samplePos /= volumeExtent;				//in range [-0.5, 0.5]
    samplePos += 0.5f;						//in range [0:1]
	return samplePos;
}