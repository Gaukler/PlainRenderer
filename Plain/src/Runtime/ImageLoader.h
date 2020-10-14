#pragma once
#include "pch.h"
#include "Rendering/ResourceDescriptions.h"

/*
if isFullPath is false, the resource directory is prepended
*/
bool loadImage(const std::filesystem::path& filename, const bool isFullPath, ImageDescription* outImage);

bool loadDDSFile(const std::filesystem::path& filename, ImageDescription* outImage);