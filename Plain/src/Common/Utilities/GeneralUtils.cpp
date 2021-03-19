#include "pch.h"
#include "GeneralUtils.h"

bool vectorContains(const std::vector<uint32_t>& vector, const uint32_t index) {
    return std::find(vector.begin(), vector.end(), index) != vector.end();
}

std::vector<char> dataToCharArray(const void* data, const size_t size) {
    std::vector<char> result(size);
    memcpy(result.data(), data, size);
    return result;
}