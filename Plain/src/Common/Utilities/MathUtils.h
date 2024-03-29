#pragma once
#include "pch.h"
#include <glm/common.hpp>

//x is phi, y is theta
glm::vec3 directionToVector(const glm::vec2 angles);
uint32_t mipCountFromResolution(const uint32_t width, const uint32_t height, const uint32_t depth);
glm::ivec3 resolutionFromMip(const glm::ivec3 baseResolution, const int mipLevel);

glm::vec2 hammersley2D(const uint32_t index);
uint32_t reverse32Bit(const uint32_t in);
float radicalInverseBase2(const uint32_t in);
float radicalInverseBase3(const uint32_t in);

//returns dot product with itself
float dot2(const glm::vec3& v);