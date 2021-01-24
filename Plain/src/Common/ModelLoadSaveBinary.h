#pragma once
#include "pch.h"
#include "Common/MeshData.h"
#include "Common/Scene.h"

//filename is relative to resource directory
void saveBinaryScene(const std::filesystem::path& filename, SceneBinary scene);

//filename is relative to resource directory
bool loadBinaryScene(const std::filesystem::path& filename, SceneBinary* outScene);