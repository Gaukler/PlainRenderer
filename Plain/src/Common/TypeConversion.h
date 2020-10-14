#pragma once
#include "pch.h"

//convert a float in range [0, 1] to uint16_t using full range
//0 maps to 0, 1 maps to max value of uint16_t
uint16_t floatToNormalizedUInt16(const float f);

//convert a float in range [-1, 1] to int16_t using full range
//-1 maps to min of int16_t, 1 maps to max value of int16_t
int16_t floatToNormalizedInt16(const float f);

//convert a float in range [-1, 1] to normalized format
//corresponds to VK_FORMAT_A2R10G10B10_SNORM_PACK32
//uint32_t only used return value because it matches 32 bit size
uint32_t vec3ToNormalizedR10B10G10A2(const glm::vec3& v);