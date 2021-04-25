#pragma once
#include "pch.h"

namespace FrameIndex {
    void markNewFrame();
    size_t getFrameIndex();
    size_t getFrameIndexMod2();
    size_t getFrameIndexMod3();
    size_t getFrameIndexMod4();
    size_t getFrameIndexMod8();
}