#include "pch.h"
#include "TypeConversion.h"

/*
=========
floatToNormalizedUInt16
=========
*/
uint16_t floatToNormalizedUInt16(const float f) {
    const float fClamped = glm::clamp(f, 0.f, 1.f);
    return (uint16_t)(fClamped * std::numeric_limits<uint16_t>::max());
}

/*
=========
floatToNormalizedInt16
=========
*/
int16_t floatToNormalizedInt16(const float f) {
    const float fClamped = glm::clamp(f, -1.f, 1.f);
    constexpr float minValue = std::numeric_limits<int16_t>::min();
    constexpr float maxValue = std::numeric_limits<int16_t>::max();
    const float valueRange = maxValue - minValue;
    const float fRemapped = fClamped * 0.5f + 0.5f; //remap to range [0, 1]
    return (int16_t)(fRemapped * valueRange + minValue);
}

uint32_t vec3ToNormalizedR10B10G10A2(const glm::vec3& v) {
    uint32_t result = 0;
    for (uint32_t i = 0; i < 3; i++) {
        //min and max of 10 bit signed integer
        const float minValue = -510;
        const float maxValue = 511;
        const float valueRange = maxValue - minValue;

        float value = v[i];
        const float clamped = glm::clamp(value, -1.f, 1.f);
        const float fRemapped = clamped * 0.5f + 0.5f; //remap to range [0, 1]
        int32_t bits = int32_t(fRemapped * valueRange + minValue);

        //get rid of bits above 10 in case of unsigned
        const int32_t bitOver10Mask = 1023;
        bits &= bitOver10Mask;

        result |= bits << ((2-i) * 10);
    }
    return result;
}