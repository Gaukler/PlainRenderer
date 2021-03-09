#pragma once
#include "pch.h"

std::vector<uint8_t> generateWhiteNoiseTexture(const glm::ivec2& resolution);
std::vector<uint8_t> generateBlueNoiseTexture(const glm::ivec2& resolution, const size_t channelCount);

//generate blue noise samples in range [0:1]
//currently discretized to 64x64 grid, because of this should only be used for samller sample counts
std::vector<glm::vec2> generateBlueNoiseSampleSequence(const uint32_t count);

//the higher the grid cell count the smaller the noise pattern
std::vector<uint8_t> generate2DPerlinNoise(const glm::ivec2& resolution, const int gridCellCount = 8);
std::vector<uint8_t> generate3DPerlinNoise(const glm::ivec3& resolution, const int gridCellCount = 8);