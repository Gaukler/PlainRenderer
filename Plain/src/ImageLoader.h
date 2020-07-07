#pragma once
#include "pch.h"
#include "Rendering/ResourceDescriptions.h"

/*
if isFullPath is false, the resource directory is prepended
*/
ImageDescription loadImage(const std::filesystem::path& filename, const bool isFullPath);