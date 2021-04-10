#ifndef LINEAR_DEPTH_INC
#define LINEAR_DEPTH_INC

//reference: https://stackoverflow.com/questions/51108596/linearize-depth
float linearizeDepth(float depth, float near, float far)
{
    return near * far / (far + (-depth+1) * (near - far));
}

#endif // #ifndef LINEAR_DEPTH_INC