#include "pch.h"
#include "FrameIndex.h"

namespace FrameIndex{

    size_t frameIndex = 0;
    size_t frameIndexMod2 = 0;
    size_t frameIndexMod3 = 0;
    size_t frameIndexMod4 = 0;
    size_t frameIndexMod8 = 0;

    void markNewFrame() {
        frameIndex++;
        frameIndexMod2 = frameIndex % 2;
        frameIndexMod3 = frameIndex % 3;
        frameIndexMod4 = frameIndex % 4;
        frameIndexMod8 = frameIndex % 8;
    }

    size_t getFrameIndex() {
        return frameIndex;
    }

    size_t getFrameIndexMod2(){
        return frameIndexMod2;
    }

    size_t getFrameIndexMod3(){
        return frameIndexMod3;
    }

    size_t getFrameIndexMod4() {
        return frameIndexMod4;
    }

    size_t getFrameIndexMod8() {
        return frameIndexMod8;
    }
}