#ifndef DYNAMIC_SUBPIXEL_JITTER_INC
#define DYNAMIC_SUBPIXEL_JITTER_INC

vec2 computeDynamicSubpixelJitterScale(vec3 posObj, mat4 mvp, mat4 mvpPrevious, vec2 currentJitter, vec2 lastJitter){

    precise vec4 previousPos = mvpPrevious * vec4(posObj, 1.f);
    previousPos /= previousPos.w;

    precise vec4 pos = mvp * vec4(posObj, 1.f);
    pos /= pos.w;

    precise vec2 motion = previousPos.xy - pos.xy;
    motion *= 0.5;

    precise vec2 jitterDistance = abs(lastJitter - currentJitter);
    precise vec2 scale = 0.5f + 0.5f * cos((pi / jitterDistance) * motion);
    scale.x = jitterDistance.x == 0 ? 1 : scale.x;
    scale.y = jitterDistance.y == 0 ? 1 : scale.y;

    return scale;
}

vec2 testJitter(vec3 posObj, mat4 mvp, mat4 mvpPrevious){
    precise vec4 previousPos = mvpPrevious * vec4(posObj, 1.f);
    previousPos /= previousPos.w;

    precise vec4 pos = mvp * vec4(posObj, 1.f);
    pos /= pos.w;

    precise vec2 motion = previousPos.xy - pos.xy;
    motion *= 0.5;

    motion /= g_screenResolution;
    motion = clamp(motion, 0, 1);

    return mix(vec2(1), vec2(1), motion);
}

#endif // #ifndef DYNAMIC_SUBPIXEL_JITTER_INC