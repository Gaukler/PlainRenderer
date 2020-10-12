#pragma once
#include "pch.h"
#include "vulkan/vulkan.h"

#include "Rendering/MeshData.h"

bool loadModelOBJ(const std::filesystem::path& filename, std::vector<MeshData>* outData);
