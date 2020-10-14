#pragma once
#include "pch.h"

//compressed types use structs for additional type safety

struct NormalizedUInt16 {
    uint16_t value;
};

struct NormalizedInt16 {
    int16_t value;
};

struct NormalizedR10G10B10A2 {
    uint32_t value;
};

//convert a float in range [0, 1] to uint16_t using full range
//0 maps to 0, 1 maps to max value of uint16_t
NormalizedUInt16 floatToNormalizedUInt16(const float f);

//convert a float in range [-1, 1] to int16_t using full range
//-1 maps to min of int16_t, 1 maps to max value of int16_t
NormalizedInt16 floatToNormalizedInt16(const float f);

//convert a float in range [-1, 1] to normalized format
//corresponds to VK_FORMAT_A2R10G10B10_SNORM_PACK32
NormalizedR10G10B10A2 vec3ToNormalizedR10B10G10A2(const glm::vec3& v);