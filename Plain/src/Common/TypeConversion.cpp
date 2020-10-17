#include "pch.h"

bool charArrayToInt(const char* in, int* outResult) {
    std::istringstream stream(in);
    if (stream >> *outResult) {
        return true;
    }
    else {
        return false;
    }
}