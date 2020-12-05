#pragma once
#include "pch.h"
#include "Resources.h"
#include "Runtime/Rendering/ResourceDescriptions.h"

bool loadShader(const std::filesystem::path pathAbsolute, std::vector<uint32_t>* outSpirV);

struct GraphicPassShaderPaths {
    std::filesystem::path                 vertex;
    std::filesystem::path                 fragment;
    std::optional<std::filesystem::path>  geometry;
    std::optional<std::filesystem::path>  tessellationControl;
    std::optional<std::filesystem::path>  tessellationEvaluation;
};

//helper that loads shaders from all supplied paths into the corresponding out struct
bool loadGraphicPassShaders(const GraphicPassShaderPaths& shaderPaths, GraphicPassShaderSpirV* outSpirV);


std::filesystem::path relativeShaderPathToAbsolute(std::filesystem::path relativePath);

//returns absolute cache path
std::filesystem::path shaderCachePathFromRelative(std::filesystem::path relativePath);

std::filesystem::path getShaderDirectory();
std::filesystem::path getShaderCacheDirectory();