#ifndef GLOBAL_INC
#define GLOBAL_INC

layout(set=0, binding = 0, std140) uniform global{
    mat4 g_viewProjection;
    mat4 g_viewProjectionPrevious;
    vec4 g_sunDirection;
    vec4 g_cameraPosition;
    vec4 g_cameraPositionPrevious;
    vec4 g_cameraRight;
    vec4 g_cameraUp;
    vec4 g_cameraForward;
    vec4 g_cameraForwardPrevious;
    ivec4 g_noiseTextureIndices;
    vec2 g_currentFrameCameraJitter;
    vec2 g_previousFrameCameraJitter;
    ivec2 g_screenResolution;
    float g_cameraTanFovHalf;
    float g_cameraAspectRatio;
    float g_nearPlane;
    float g_farPlane;
    float g_sunStrength;
    float g_exposureOffset;
    float g_exposureAdaptionSpeedEvPerSec;
    float g_deltaTime;
    float g_time;
    float g_mipBias;
    bool g_cameraCut;
    uint g_frameIndex;
    uint g_frameIndexMod2;
    uint g_frameIndexMod3;
    uint g_frameIndexMod4;
};

layout(set=0, binding = 1) uniform sampler g_sampler_anisotropicRepeat;
layout(set=0, binding = 2) uniform sampler g_sampler_nearestBlackBorder;
layout(set=0, binding = 3) uniform sampler g_sampler_linearRepeat;
layout(set=0, binding = 4) uniform sampler g_sampler_linearClamp;
layout(set=0, binding = 5) uniform sampler g_sampler_nearestClamp;
layout(set=0, binding = 6) uniform sampler g_sampler_linearWhiteBorder;
layout(set=0, binding = 7) uniform sampler g_sampler_nearestRepeat;
layout(set=0, binding = 8) uniform sampler g_sampler_nearestWhiteBorder;

float pi = 3.1415926535f;

#endif // #ifndef GLOBAL_INC