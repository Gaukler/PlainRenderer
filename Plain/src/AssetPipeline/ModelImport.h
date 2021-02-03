#pragma once
#include "pch.h"
#include "Common/MeshData.h"
#include "Scene.h"

bool loadModelGLTF(const std::filesystem::path& filename, Scene* outScene);