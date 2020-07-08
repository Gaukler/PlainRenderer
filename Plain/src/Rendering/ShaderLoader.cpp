#include "ShaderLoader.h"
#include "Utilities/DirectoryUtils.h"

/*
=========
loadShaderFile
=========
*/
std::vector<char> loadShaderFile(const std::filesystem::path& filename) {

    DirectoryUtils directories = DirectoryUtils::getReference();
    const std::filesystem::path path = directories.getResourceDirectory();
    const std::filesystem::path fullPath = path / filename;

    std::ifstream file(fullPath, std::ios::in | std::ios::ate);
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