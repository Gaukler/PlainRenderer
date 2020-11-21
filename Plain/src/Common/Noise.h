#pragma once
#include "pch.h"

std::vector<uint8_t> generateWhiteNoiseTexture(const glm::ivec2& resolution);
std::vector<uint8_t> generateBlueNoiseTexture(const glm::ivec2& resolution);

//generate blue noise samples in range [0:1]
//currently discretized to 64x64 grid, because of this should only be used for samller sample counts
std::vector<glm::vec2> generateBlueNoiseSampleSequence(const uint32_t count);