#include "ShaderIO.h"
#include "Utilities/DirectoryUtils.h"
#include "ShaderCompile.h"

bool loadShader(const ShaderDescription& desc, std::vector<uint32_t>* outSpirV) {

    const auto absolutePath = absoluteShaderPathFromRelative(desc.srcPathRelative);
    std::filesystem::path spirVCachePath = shaderCachePathFromRelative(desc.srcPathRelative);

    if (std::filesystem::exists(spirVCachePath)) {
        const auto sourceLastChange = std::filesystem::last_write_time(absolutePath);
        const auto cacheLastChange  = std::filesystem::last_write_time(spirVCachePath);
        if (sourceLastChange > cacheLastChange) {
            std::cout << desc.srcPathRelative << " cache out of date, recompiling\n";
        }
        else {
            return loadShaderSpirVFile(spirVCachePath, outSpirV);
        }
    }
    else {
        std::cout << desc.srcPathRelative << " no cache found, recompiling\n";
    }

    std::vector<char> shaderGLSL;
    bool success = loadShaderTextFile(absolutePath, &shaderGLSL);
    success &= compileGLSLToSPIRV(shaderGLSL, absolutePath, outSpirV);
    if (!success) {
        //modify the last write time to prevent trying to load the unusable file again without update
        std::filesystem::last_write_time(spirVCachePath, std::filesystem::last_write_time(absolutePath));
        return false;
    }

    writeSpirVToFile(*outSpirV, spirVCachePath);
    return true;
}

bool loadGraphicPassShaders(const GraphicPassShaderDescriptions& shaderDescriptions, GraphicPassShaderSpirV* outSpirV) {
    bool success = true;
    success &= loadShader(shaderDescriptions.vertex, &outSpirV->vertex);
    success &= loadShader(shaderDescriptions.fragment, &outSpirV->fragment);
    if (shaderDescriptions.geometry.has_value()) {
        success &= loadShader(shaderDescriptions.geometry.value(), &outSpirV->geometry.value());
    }
    if (shaderDescriptions.tesselationControl.has_value()) {
        success &= loadShader(shaderDescriptions.tesselationControl.value(), &outSpirV->tesselationControl.value());
    }
    if (shaderDescriptions.tesselationEvaluation.has_value()) {
        success &= loadShader(shaderDescriptions.tesselationEvaluation.value(), &outSpirV->tesselationEvaluation.value());
    }
    return success;
}

bool loadShaderTextFile(const std::filesystem::path& absolutePath, std::vector<char>* outShaderCode) {

    std::ifstream file(absolutePath, std::ios::in | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "could not open file " << absolutePath << std::endl;
        return false;
    }

    //read file into buffer
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    outShaderCode->resize(fileSize);
    file.read(outShaderCode->data(), fileSize);

    file.close();

    return true;
}

bool loadShaderSpirVFile(const std::filesystem::path& absolutePath, std::vector<uint32_t>* outSpirV) {
    std::ifstream file(absolutePath, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "Could not open file " << absolutePath << std::endl;
        return false;
    }

    //read file into buffer
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    outSpirV->resize(fileSize / 4);
    file.read(reinterpret_cast<char*>(outSpirV->data()), fileSize);
    file.close();

    return true;
}

void writeSpirVToFile(const std::vector<uint32_t>& spirV, const std::filesystem::path absolutePath) {
    std::ofstream spirVFile;
    spirVFile.open(absolutePath, std::ios::out | std::ios::binary);
    assert(spirVFile.is_open());
    spirVFile.write(reinterpret_cast<const char*>(spirV.data()), spirV.size() * sizeof(uint32_t));
    spirVFile.close();
}

std::filesystem::path absoluteShaderPathFromRelative(std::filesystem::path relativePath) {
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
    return DirectoryUtils::getResourceDirectory() / "shaders\\cache\\";
}