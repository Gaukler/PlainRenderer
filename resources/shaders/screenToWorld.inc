#ifndef SCREEN_TO_WORLD
#define SCREEN_TO_WORLD

vec3 calculateViewDirectionFromPixel(vec2 pixelNDC, vec3 cameraForward, vec3 cameraUp, vec3 cameraRight, float cameraTanFovHalf, float aspectRatio){
    vec3 V = -cameraForward;
    V += cameraTanFovHalf * pixelNDC.y * cameraUp;
    V -= cameraTanFovHalf * aspectRatio * pixelNDC.x * cameraRight;
    return normalize(V);
}

#endif // #ifndef SCREEN_TO_WORLD