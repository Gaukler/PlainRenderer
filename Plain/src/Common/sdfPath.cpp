#pragma once
#include "pch.h"

std::filesystem::path binaryToSDFPath(const std::filesystem::path binaryPathRelative) {
    std::filesystem::path sdfPathRelative = binaryPathRelative;
    sdfPathRelative.replace_extension(); //remove extension
    sdfPathRelative += "_sdf";
    sdfPathRelative.replace_extension("dds");
    return sdfPathRelative;
}