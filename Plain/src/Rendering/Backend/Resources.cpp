#include "pch.h"
#include "Resources.h"

//enum class bit operators
MaterialFeatureFlags operator&(const MaterialFeatureFlags l, const MaterialFeatureFlags r) {
    return MaterialFeatureFlags(uint32_t(l) & uint32_t(r));
}

MaterialFeatureFlags operator|(const MaterialFeatureFlags l, const MaterialFeatureFlags r) {
    return MaterialFeatureFlags(uint32_t(l) | uint32_t(r));
}