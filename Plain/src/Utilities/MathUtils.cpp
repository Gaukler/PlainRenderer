#include "pch.h"
#include "MathUtils.h"

/*
=========
directionToVector
=========
*/
glm::vec3 directionToVector(const glm::vec2 direction) {

    const float theta   = direction.y / 180.f * 3.1415f;
    const float phi     = direction.x / 180.f * 3.1415f;

    glm::vec3 vec;
    vec.x = sin(theta) * cos(phi);
    vec.y = sin(theta) * sin(phi);
    vec.z = cos(theta);
    return vec;
}

/*
=========
mipCountFromResolution
=========
*/
uint32_t mipCountFromResolution(const uint32_t width, const uint32_t height, const uint32_t depth) {
    return 1 + std::floor(std::log2(std::max(std::max(width, height), depth)));
}