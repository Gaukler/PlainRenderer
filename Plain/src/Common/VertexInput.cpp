#include "pch.h"
#include "VertexInput.h"

//enum class bit operators
VertexInputFlags operator&(const VertexInputFlags l, const VertexInputFlags r) {
    return VertexInputFlags(uint32_t(l) & uint32_t(r));
}

VertexInputFlags operator|(const VertexInputFlags l, const VertexInputFlags r) {
    return VertexInputFlags(uint32_t(l) | uint32_t(r));
}