#include "FileIO.h"

bool checkLastChangeTime(const fs::path path, fs::file_time_type* outLastChangeTime) {
    std::error_code exception;
    const fs::file_time_type lastWriteTimeSrc = std::filesystem::last_write_time(path, exception);
    if (exception.value() != 0) {
        std::cout << "Failed to to read file last change time\n";
        std::cout << exception.message() << std::endl;
        return false;
    }
    *outLastChangeTime = lastWriteTimeSrc;
    return true;
}

bool loadTextFile(const std::filesystem::path& absolutePath, std::vector<char>* outText) {

    std::ifstream file(absolutePath, std::ios::in | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "could not open file " << absolutePath << std::endl;
        return false;
    }

    //read file into buffer
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    outText->resize(fileSize);
    file.read(outText->data(), fileSize);

    file.close();

    return true;
}

bool loadBinaryFile(const std::filesystem::path& absolutePath, std::vector<uint32_t>* outData) {
    std::ifstream file(absolutePath, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cout << "Could not open file " << absolutePath << std::endl;
        return false;
    }

    //read file into buffer
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    outData->resize(fileSize / 4); //4 chars per size_t, TODO: think about systems with different data sizes
    file.read(reinterpret_cast<char*>(outData->data()), fileSize);
    file.close();

    return true;
}

void writeBinaryFile(const std::filesystem::path absolutePath, const std::vector<uint32_t>& data) {
    std::ofstream spirVFile;
    spirVFile.open(absolutePath, std::ios::out | std::ios::binary);
    assert(spirVFile.is_open());
    spirVFile.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(uint32_t));
    spirVFile.close();
}