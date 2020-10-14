#pragma once
#include "pch.h"

bool vectorContains(const std::vector<uint32_t>& vector, const uint32_t pass);

//copies arbitrary data to a char vector
std::vector<char> dataToCharArray(void* data, const size_t size);