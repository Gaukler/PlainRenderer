//reference: https://stackoverflow.com/questions/51108596/linearize-depth
float linearizeDepth(float depth, float near, float far)
{
    return near * far / (far + depth * (near - far));
}