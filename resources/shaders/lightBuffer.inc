#ifndef LIGHT_BUFFER_INC
#define LIGHT_BUFFER_INC

struct LightBuffer{
    vec3 sunColor;
    float previousFrameExposure;
    float sunStrengthExposed;
};

#endif // #ifndef LIGHT_BUFFER_INC