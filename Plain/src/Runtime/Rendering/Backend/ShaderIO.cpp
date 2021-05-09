#include "ShaderIO.h"
#include "Utilities/DirectoryUtils.h"
#include "ShaderCompile.h"
#include "Common/FileIO.h"

bool loadShader(const std::filesystem::path pathAbsolute, std::vector<uint32_t>* outSpirV) {
    std::vector<char> shaderGLSL;
    bool success = loadTextFile(pathAbsolute, &shaderGLSL);
    if (success) {
        success &= compileGLSLToSPIRV(shaderGLSL, pathAbsolute, outSpirV);
    }
    if (!success) {
        return false;
    }
    return true;
}


bool loadGraphicPassShaders(const GraphicPassShaderPaths& shaderPaths, GraphicPassShaderSpirV* outSpirV) {
    bool success = true;
    success &= loadShader(shaderPaths.vertex,   &outSpirV->vertex);
    success &= loadShader(shaderPaths.fragment, &outSpirV->fragment);
    if (shaderPaths.geometry.has_value()) {
        outSpirV->geometry = std::vector<uint32_t>();
        success &= loadShader(shaderPaths.geometry.value(), &outSpirV->geometry.value());
    }
    if (shaderPaths.tessCtrl.has_value()) {
        outSpirV->tessCtrl = std::vector<uint32_t>();
        success &= loadShader(shaderPaths.tessCtrl.value(), &outSpirV->tessCtrl.value());
    }
    if (shaderPaths.tessEval.has_value()) {
        outSpirV->tessEval = std::vector<uint32_t>();
        success &= loadShader(shaderPaths.tessEval.value(), &outSpirV->tessEval.value());
    }
    return success;
}

std::filesystem::path relativeShaderPathToAbsolute(std::filesystem::path relativePath) {
    return getShaderDirectory() / relativePath;
}

std::filesystem::path shaderCachePathFromRelative(std::filesystem::path relativePath) {
    std::filesystem::path shaderCachePath = getShaderCacheDirectory() / relativePath;
    shaderCachePath += ".spv";
    return shaderCachePath;
}

std::filesystem::path getShaderDirectory() {
    return DirectoryUtils::getResourceDirectory() / "shaders\\";
}

std::filesystem::path getShaderCacheDirectory() {
    return DirectoryUtils::getResourceDirectory() / "shaderCache\\";
}