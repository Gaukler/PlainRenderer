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
    const float minValue = std::numeric_limits<int16_t>::min();
    const float maxValue = std::numeric_limits<int16_t>::max();
    const float valueRange = maxValue - minValue;
    const float fRemapped = fClamped * 0.5f + 0.5f; //remap to range [0, 1]
    return (int16_t)(fRemapped * valueRange + minValue);
}