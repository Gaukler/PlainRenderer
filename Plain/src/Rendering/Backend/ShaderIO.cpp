#include "ShaderIO.h"
#include "Utilities/DirectoryUtils.h"
#include "ShaderCompile.h"

/*
=========
loadShader
=========
*/
std::vector<uint32_t> loadShader(const std::filesystem::path& relativePath) {

    const auto absolutePath = getShaderDirectory() / relativePath;
    std::filesystem::path spirVCachePath = getShaderCacheDirectory() / relativePath;
    spirVCachePath += ".spv";

    if (std::filesystem::exists(spirVCachePath)) {
        const auto sourceLastChange = std::filesystem::last_write_time(absolutePath);
        const auto cacheLastChange  = std::filesystem::last_write_time(spirVCachePath);
        if (sourceLastChange > cacheLastChange) {
            std::cout << relativePath << " cache out of date, recompiling\n";
        }
        else {
            return loadShaderSpirVFile(spirVCachePath);
        }
    }
    else {
        std::cout << relativePath << " no cache found, recompiling\n";
    }

    const auto shaderGLSL = loadShaderTextFile(absolutePath);
    const auto spirV = compileGLSLToSPIRV(shaderGLSL, absolutePath);
    
    writeSpirVToFile(spirV, spirVCachePath);

    return spirV;
}

/*
=========
getShaderDirectory
=========
*/
std::filesystem::path getShaderDirectory() {
    return DirectoryUtils::getResourceDirectory() / "shaders\\";
}

/*
=========
getShaderCacheDirectory
=========
*/
std::filesystem::path getShaderCacheDirectory() {
    return DirectoryUtils::getResourceDirectory() / "shaders\\cache\\";
}

/*
=========
loadShaderFile
=========
*/
std::vector<char> loadShaderTextFile(const std::filesystem::path& absolutePath) {

    std::ifstream file(absolutePath, std::ios::in | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("could not open file");
    }

    //read file into buffer
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(fileSize);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

std::vector<uint32_t> loadShaderSpirVFile(const std::filesystem::path& absolutePath) {
    std::ifstream file(absolutePath, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("could not open file");
    }

    //read file into buffer
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint32_t> buffer(fileSize / 4);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    return buffer;
}

/*
=========
writeSpirVToFile
=========
*/
void writeSpirVToFile(const std::vector<uint32_t>& spirV, const std::filesystem::path absolutePath) {
    std::ofstream spirVFile;
    spirVFile.open(absolutePath, std::ios::out | std::ios::binary);
    assert(spirVFile.is_open());
    spirVFile.write(reinterpret_cast<const char*>(spirV.data()), spirV.size() * sizeof(uint32_t));
    spirVFile.close();
}